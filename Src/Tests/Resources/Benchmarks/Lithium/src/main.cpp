//
// Copyright (C) 2025 Glauco Pacheco <glauco@kourier.io>
// SPDX-License-Identifier: AGPL-3.0-only
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as
// published by the Free Software Foundation, version 3 of the License.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
//

#include "lithium_http_server.hh"
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QStringList>

using namespace li;


int main(int argc, char ** argv)
{
  QStringList args;
  for (auto i = 0; i < argc; ++i)
    args.append(QString::fromUtf8(argv[i]));
  QCommandLineParser cmdLineParser;
  cmdLineParser.addHelpOption();
  cmdLineParser.addOption({"worker-count", "Makes server use <N> workers. Given value must be a positive integer", "N", "-1"});
  cmdLineParser.process(args);
  bool conversionSucceeded = false;
  const int64_t workerCount = cmdLineParser.value("worker-count").toLongLong(&conversionSucceeded);
  if (!conversionSucceeded)
    cmdLineParser.showHelp(1);
  else
  {
    if (workerCount <= 0)
      cmdLineParser.showHelp(1);
    else
      qInfo("Using %d workers.", workerCount);
  }
  http_api httpApi;
  httpApi.global_handler() =
  [&](http_request& request, http_response& response) {
    response.write("Hello World!");
  };
  http_serve(httpApi, 8250, s::nthreads = workerCount);
  return 0;
}
