/**
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef __STOUT_OS_WINDOWS_MKDTEMP_HPP__
#define __STOUT_OS_WINDOWS_MKDTEMP_HPP__

#include <time.h>

#include <stout/try.hpp>
#include <stout/nothing.hpp>

#include <stout/os/mkdir.hpp>


namespace os {

// Creates a temporary directory using the specified path
// template. The template may be any path with _6_ `Xs' appended to
// it, for example /tmp/temp.XXXXXX. The trailing `Xs' are replaced
// with a unique alphanumeric combination.
inline Try<std::string> mkdtemp(const std::string& path = "/tmp/XXXXXX")
{
  static const char postfixTemplate[] = "XXXXXX";
  static const size_t postfixSize = sizeof(postfixTemplate) - 1;

  // NOTE: this will just be a relative path if the template length is equal
  // to path length.
  if (path.length() < postfixSize) {
    return Error(
      "Invalid template passed to `os::mkdtemp`: template '" + path +
      "' should end with 6 'X' characters, but length was less than 6 total.");
  }

  static const std::string alphabet =
    "0123456789"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz";
  static const size_t alphSize = alphabet.length() - 1;

  // NOTE: Generating postfix with good-enough, not-cryptographically-secure,
  // plain-old calls to `rand`.
  char postfix[postfixSize + 1];
  srand(time(NULL));

  for (int i = 0; i < postfixSize; ++i) {
    int index = rand() % alphSize;
    postfix[i] = alphabet[index];
  }

  postfix[postfixSize] = '\0';

  // Validate template is correctly formatted.
  const int templateFormatCorrect =
    path.compare(
      path.length() - postfixSize,
      postfixSize,
      postfixTemplate)
    == 0;

  if (!templateFormatCorrect) {
    return Error(
      "Invalid template passed to `os::mkdtemp`: template '" + path +
      "' does not end with 6 'X' characters.");
  }

  // Replace template, make directory.
  std::string tempPath = path
    .substr(0, path.length() - postfixSize)
    .append(postfix);

  Try<Nothing> tmpDirCreated = os::mkdir(tempPath);

  if (tmpDirCreated.isError()) {
    return Error(tmpDirCreated.error());
  }

  return tempPath;
}

} // namespace os {


#endif // __STOUT_OS_WINDOWS_MKDTEMP_HPP__
