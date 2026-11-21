/*
 * Copyright (C) 2025 The Coral Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <utils/Path.h>

#include <vector>

#include <dirent.h>
#include <limits.h>
#include <pwd.h>
#include <stdlib.h>
#include <sys/stat.h>

#include <mach-o/dyld.h>

#include <Foundation/Foundation.h>

namespace utils {

bool Path::mkdir() const {
  return ::mkdir(mPath.c_str(), S_IRUSR | S_IWUSR | S_IXUSR) == 0;
}

Path Path::getCurrentExecutable() {
  // `_NSGetExecutablePath` writes at most `size` bytes and updates it with the
  // required length (including the terminator) when the buffer is too small, so
  // ask for the size first instead of guessing a fixed 2 KiB.
  uint32_t bufferSize = 0;
  _NSGetExecutablePath(nullptr, &bufferSize);
  if (bufferSize == 0) {
    return {};
  }

  std::vector<char> executable(bufferSize);
  if (_NSGetExecutablePath(executable.data(), &bufferSize) != 0) {
    return {};
  }

  // The result is the path that launched the process: it can be relative and can
  // still contain symlinks. Resolve both so a caller can compare it against the
  // real install location.
  char resolved[PATH_MAX];
  if (realpath(executable.data(), resolved) != nullptr) {
    return Path(resolved);
  }
  return Path(executable.data());
}

Path Path::getTemporaryDirectory() {
  @autoreleasepool {
    NSString* tempDir = NSTemporaryDirectory();
    if (tempDir == nil) {
      // Foundation could not name a temporary directory (for example in a very
      // restricted process); fall back to the POSIX default rather than passing
      // null to Path.
      return Path("/tmp");
    }
    return Path([tempDir cStringUsingEncoding:NSUTF8StringEncoding]);
  }
}

Path Path::getUserSettingsDirectory() {
  @autoreleasepool {
    // The platform-correct location for application support data. On macOS this
    // is ~/Library/Application Support and on iOS it is the same directory inside
    // the application container, which is backed up and not purged like Caches.
    NSArray<NSString*>* directories =
        NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory, NSUserDomainMask, YES);
    if (directories.count > 0) {
      NSString* directory = directories.firstObject;
      if (directory != nil) {
        Path result([directory cStringUsingEncoding:NSUTF8StringEncoding]);
        // iOS does not create Application Support in the application container.
        // The method promises a usable directory, so create it on first use; a
        // caller that only reads the path is unaffected.
        if (!result.isEmpty() && !result.exists()) {
          (void) result.mkdirRecursive();
        }
        return result;
      }
    }

    NSString* home = NSHomeDirectory();
    if (home != nil) {
      return Path([home cStringUsingEncoding:NSUTF8StringEncoding]);
    }
  }

  // Last resort for a process without a Foundation home directory.
  const char* home = getenv("HOME");
  if (home == nullptr) {
    struct passwd* pwd = getpwuid(getuid());
    if (pwd != nullptr) {
      home = pwd->pw_dir;
    }
  }
  return home != nullptr ? Path(home) : Path();
}

std::vector <Path> Path::listContents() const {
  // Return an empty vector if the path doesn't exist or is not a directory
  if (!isDirectory() || !exists()) {
    return {};
  }

  struct dirent* directory;
  DIR* dir;

  dir = opendir(c_str());
  if (dir == nullptr) {
    // Path does not exist or could not be read
    return {};
  }

  std::vector <Path> directory_contents;

  while ((directory = readdir(dir)) != nullptr) {
    const char* file = directory->d_name;
    if (file[0] != '.') {
      directory_contents.push_back(concat(directory->d_name));
    }
  }

  closedir(dir);
  return directory_contents;
}

} // namespace utils
