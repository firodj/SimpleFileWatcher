/**
	Copyright (c) 2009 James Wynn (james@jameswynn.com)

	Permission is hereby granted, free of charge, to any person obtaining a copy
	of this software and associated documentation files (the "Software"), to deal
	in the Software without restriction, including without limitation the rights
	to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
	copies of the Software, and to permit persons to whom the Software is
	furnished to do so, subject to the following conditions:

	The above copyright notice and this permission notice shall be included in
	all copies or substantial portions of the Software.

	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
	OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
	THE SOFTWARE.

	James Wynn james@jameswynn.com
*/

#include <FileWatcher/FileWatcherOSX.h>

#if FILEWATCHER_PLATFORM == FILEWATCHER_PLATFORM_KQUEUE

#include <memory>
#include <vector>
#include <algorithm>
#include <sys/event.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>

char *flagstring(int flags)
{
    static char ret[512];
    const char *stror = "";

    ret[0]='\0'; // clear the string.
    if (flags & NOTE_DELETE) {strcat(ret,stror);strcat(ret,"NOTE_DELETE");stror="|";}
    if (flags & NOTE_WRITE) {strcat(ret,stror);strcat(ret,"NOTE_WRITE");stror="|";}
    if (flags & NOTE_EXTEND) {strcat(ret,stror);strcat(ret,"NOTE_EXTEND");stror="|";}
    if (flags & NOTE_ATTRIB) {strcat(ret,stror);strcat(ret,"NOTE_ATTRIB");stror="|";}
    if (flags & NOTE_LINK) {strcat(ret,stror);strcat(ret,"NOTE_LINK");stror="|";}
    if (flags & NOTE_RENAME) {strcat(ret,stror);strcat(ret,"NOTE_RENAME");stror="|";}
    if (flags & NOTE_REVOKE) {strcat(ret,stror);strcat(ret,"NOTE_REVOKE");stror="|";}

    return ret;
}

namespace FW
{
	typedef struct kevent KEvent;

	struct EntryStruct
	{
		EntryStruct(std::string filename, FW::WatchID watchid, time_t mtime = 0)
		: mFilename(filename), mWatchID(watchid), mModifiedTime(mtime)
		{
		}
		~EntryStruct()
		{
		}
		std::string mFilename;
		time_t mModifiedTime;
		FW::WatchID mWatchID;
	};

	struct WatchStruct
	{
		WatchID mWatchID;
		String mDirName;
		FileWatchListener* mListener;

		// index 0 is always the directory
		std::vector<KEvent> mChangeList;
		std::map<String, std::unique_ptr<EntryStruct>> mEntryList;

		WatchStruct(WatchID watchid, const String& dirname, FileWatchListener* listener)
		: mWatchID(watchid), mDirName(dirname), mListener(listener)
		{
			addAll();
		}

		void addFile(const String& name, bool imitEvents = true)
		{
			//fprintf(stderr, "ADDED: %s\n", name.c_str());

			// create entry
			struct stat attrib;
			stat(name.c_str(), &attrib);

			int fd = open(name.c_str(), O_RDONLY | O_EVTONLY);

			if(fd == -1)
				throw FileNotFoundException(name);

			size_t lastIndex = mChangeList.size();
			mChangeList.emplace_back();

			mEntryList[name] = std::unique_ptr<EntryStruct>(new EntryStruct( name, mWatchID, attrib.st_mtime) );

			// set the event data at the end of the list
			EV_SET(&mChangeList[lastIndex], fd, EVFILT_VNODE,
				   EV_ADD | EV_ENABLE | EV_ONESHOT/* | EV_CLEAR*/,
				   NOTE_DELETE | NOTE_EXTEND | NOTE_WRITE | NOTE_ATTRIB | NOTE_RENAME | NOTE_LINK | NOTE_REVOKE,
				   0, (void*)mEntryList[name].get());

			// handle action
			if(imitEvents)
				handleAction(name, Actions::Add);
		}

		void removeFile(const String& name, bool imitEvents = true)
		{
			std::vector<KEvent>::iterator keit = mChangeList.end();
			// FIXME: assuming +1 (the 0 is the DIR that have NULLptr data)
			for (auto it = mChangeList.begin()+1; it != mChangeList.end(); ++it) {
				if (((EntryStruct*)it->udata)->mFilename == name) {
					keit = it; break;
				}
			}
			if(keit == mChangeList.end())
				throw FileNotFoundException(name);

			//tempEntry.mFilename = 0;

			// delete
			close(keit->ident);
			mEntryList.erase(name);
			//delete((EntryStruct*)keit->udata);
			mChangeList.erase(keit);

			// handle action
			if(imitEvents)
				handleAction(name, Actions::Delete);
		}

		// called when the directory is actually changed
		// means a file has been added or removed
		// rescans the watched directory adding/removing files and sending notices
		void rescan()
		{
			// if new file, call addFile
			// if missing file, call removeFile
			// if timestamp modified, call handleAction(filename, ACTION_MODIFIED);
			DIR* dir = opendir(mDirName.c_str());
			if(!dir)
				return;

			struct dirent* dentry;
			KEvent* ke = &mChangeList[1];
			EntryStruct* entry = 0;
			struct stat attrib;

			while((dentry = readdir(dir)) != NULL)
			{
				String fname = mDirName + "/" + dentry->d_name;
				stat(fname.c_str(), &attrib);
				if(!S_ISREG(attrib.st_mode))
					continue;
#if 0
				if(ke <= &mChangeList[mChangeListCount])
				{
					entry = (EntryStruct*)ke->udata;
					int result = strcmp(entry->mFilename, fname.c_str());
					//fprintf(stderr, "[%s cmp %s]\n", entry->mFilename, fname.c_str());
					if(result == 0)
					{
						stat(entry->mFilename, &attrib);
						time_t timestamp = attrib.st_mtime;

						if(entry->mModifiedTime != timestamp)
						{
							entry->mModifiedTime = timestamp;
							handleAction(entry->mFilename, Actions::Modified);
						}
						ke++;
					}
					else if(result < 0)
					{
						// f1 was deleted
						removeFile(entry->mFilename);
						ke++;
					}
					else
					{
						// f2 was created
						addFile(fname);
						ke++;
					}
				}
				else
				{
					// just add
					addFile(fname);
					ke++;
				}
#endif
			}//end while

			closedir(dir);
		};

		void handleAction(const String& filename, FW::Action action)
		{
			mListener->handleFileAction(mWatchID, mDirName, filename, action);
		}

		void addAll()
		{
			size_t lastIndex = mChangeList.size();
			mChangeList.emplace_back();

			// add base dir
			int fd = open(mDirName.c_str(), O_RDONLY | O_EVTONLY);
			EV_SET(&mChangeList[lastIndex], fd, EVFILT_VNODE,
				   EV_ADD | EV_ENABLE | EV_ONESHOT,
				   NOTE_DELETE | NOTE_EXTEND | NOTE_WRITE | NOTE_ATTRIB,
				   0, 0);

			//fprintf(stderr, "ADDED: %s\n", mDirName.c_str());

			// scan directory and call addFile(name, false) on each file
			DIR* dir = opendir(mDirName.c_str());
			if(!dir)
				throw FileNotFoundException(mDirName);

			struct dirent* entry;
			struct stat attrib;
			while((entry = readdir(dir)) != NULL)
			{
				String fname = (mDirName + "/" + String(entry->d_name));
				stat(fname.c_str(), &attrib);
				if(S_ISREG(attrib.st_mode))
					addFile(fname, false);
				//else
				//	fprintf(stderr, "NOT ADDED: %s (%d)\n", fname.c_str(), attrib.st_mode);

			}//end while

			closedir(dir);
		}

		void removeAll()
		{
			//KEvent* ke = NULL;

			// go through list removing each file and sending an event
			for(auto it = mChangeList.begin(); it != mChangeList.end(); ++it)
			{
				//ke = &mChangeList[i];
				//handleAction(name, Action::Delete);
				EntryStruct* entry = (EntryStruct*)it->udata;

				handleAction(entry->mFilename, Actions::Delete);

				// delete
				close(it->ident);
				mEntryList.erase(entry->mFilename);
				//delete((EntryStruct*)it->udata);
			}

			mChangeList.clear();
		}
	};

	void FileWatcherOSX::update()
	{
		int nev = 0;
		struct kevent event;

		WatchMap::iterator iter = mWatches.begin();
		WatchMap::iterator end = mWatches.end();
		for(; iter != end; ++iter)
		{
			WatchStruct* watch = iter->second;

			while((nev = kevent(mDescriptor, &watch->mChangeList[0], watch->mChangeList.size(), &event, 1, &mTimeOut)) != 0)
			{
				if(nev == -1)
					perror("kevent");
				else if ( nev > 0 )
				{
					fprintf(stderr, "FFlags: %s, Data: 0x%lx\n", flagstring(event.fflags), event.data);

					EntryStruct* entry = 0;
					if((entry = (EntryStruct*)event.udata) != 0)
					{

						// the watchID of the entry doesn't match the watchID of the WatchStruct.
						// instead, find it in the map and return it here.
						watch = mWatches[ entry->mWatchID ];
						if ( !watch )
						{
							//fprintf( stderr, "Unable to find watchID: %u\n", (unsigned int)entry->mWatchID );
							continue;
						}

						if ( event.filter == EVFILT_VNODE )
						{
							fprintf(stderr, "\tWatch: %u, File: %s, FFlags: %s", (unsigned int)watch->mWatchID, entry->mFilename.c_str(), flagstring(event.fflags));
#if 0
							if(event.fflags & NOTE_DELETE)
							{
								fprintf(stderr, "File deleted\n");
								#if 0
								//watch->handleAction(entry->mFilename, FW::Actions::Delete);
								//watch->removeFile(entry->mFilename);
								watch->rescan();
								#endif
							}
							if(event.fflags & NOTE_EXTEND ||
							   event.fflags & NOTE_WRITE ||
							   event.fflags & NOTE_ATTRIB)
							{
								fprintf(stderr, "File modified\n");
								#if 0
								//watch->rescan();
								struct stat attrib;
								stat(entry->mFilename, &attrib);
								entry->mModifiedTime = attrib.st_mtime;
								watch->handleAction(entry->mFilename, FW::Actions::Modified);
								#endif
							}
#endif

						}

					}
					else
					{
						fprintf(stderr, "Dir: %s -- rescanning\n", watch->mDirName.c_str());
						#if 0
						watch->rescan();
						#endif
					}
				}

				mTimeOut.tv_sec = 0;
				mTimeOut.tv_nsec = 0;
			}
		}
	}

	//--------
	FileWatcherOSX::FileWatcherOSX()
	{
		mDescriptor = kqueue();
		mTimeOut.tv_sec = 0;
		mTimeOut.tv_nsec = 0;
	}

	//--------
	FileWatcherOSX::~FileWatcherOSX()
	{
		WatchMap::iterator iter = mWatches.begin();
		WatchMap::iterator end = mWatches.end();
		for(; iter != end; ++iter)
		{
			delete iter->second;
		}
		mWatches.clear();

		close(mDescriptor);
	}

	//--------
	WatchID FileWatcherOSX::addWatch(const String& directory, FileWatchListener* watcher, bool recursive)
	{
		WatchStruct* watch = new WatchStruct(++mLastWatchID, directory, watcher);
		mWatches.insert(std::make_pair(mLastWatchID, watch));
		return mLastWatchID;
	}

	//--------
	void FileWatcherOSX::removeWatch(const String& directory)
	{
		WatchMap::iterator iter = mWatches.begin();
		WatchMap::iterator end = mWatches.end();
		for(; iter != end; ++iter)
		{
			if(directory == iter->second->mDirName)
			{
				removeWatch(iter->first);
				return;
			}
		}
	}

	//--------
	void FileWatcherOSX::removeWatch(WatchID watchid)
	{
		WatchMap::iterator iter = mWatches.find(watchid);

		if(iter == mWatches.end())
			return;

		WatchStruct* watch = iter->second;
		mWatches.erase(iter);

		//inotify_rm_watch(mFD, watchid);

		delete watch;
		watch = 0;
	}

	//--------
	void FileWatcherOSX::handleAction(WatchStruct* watch, const String& filename, unsigned long action)
	{
	}

};//namespace FW

#endif//FILEWATCHER_PLATFORM_KQUEUE
