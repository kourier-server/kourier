//
// Copyright (C) 2024 Glauco Pacheco <glauco@kourier.io>
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

#ifndef KOURIER_TIMER_H
#define KOURIER_TIMER_H

#include "Object.h"
#include <chrono>
#include <memory>


namespace Test::TimerNotifier {class TimerNotifierTest;}

namespace Kourier
{
class TimerPrivate;

class KOURIER_EXPORT Timer : public Object
{
KOURIER_OBJECT(Kourier::Timer)
public:
    enum class TimerType : uint8_t {Precise, Coarse, VeryCoarse};
    Timer();
    ~Timer() override;
    void start();
    void start(std::chrono::milliseconds interval);
    void stop();
    Signal timeout();
    bool isActive() const;
    bool isSingleShot() const;
    void setSingleShot(bool singleShot);
    std::chrono::milliseconds interval() const;
    void setInterval(std::chrono::milliseconds interval);
    TimerType timerType() const;
    void setTimerType(TimerType timerType);

private:
    std::unique_ptr<TimerPrivate> d_ptr;
    Q_DECLARE_PRIVATE(Timer)
    friend class Test::TimerNotifier::TimerNotifierTest;
};

}

#endif // KOURIER_TIMER_H
