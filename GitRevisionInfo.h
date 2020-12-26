/* Pre-build event in Visual Studio: GitWCRev.exe . GitRevisionInfo.tmpl GitRevisionInfo.h
 * Creates GitRevisionInfo.c so we can track Git version in the binary.
 * SVN Method devised at the suggestion of Desmond (d3s) <admin@quake2lithium.com>
 * This requires TortoiseGit installed on machines used to build the project.
 * Get TortoiseGit at http://tortoiseGit.net/
 */
 
 /* Get the current Git revision number and make it a string we can
  * access in the project. GitRevisionInfo.h will be made from this
  * template, keep it un-versioned but in the project.
  */

#pragma once

#ifdef _WIN32
#define OPENRA2_REVISION 318	// Equivalent to git rev-list --count HEAD
#define OPENRA2_VERSION "1c8cf03"	// Equivalent to git rev-parse --short HEAD
#define COPYRIGHT "Copyright 2016-2020 packetflinger.com"
#endif
