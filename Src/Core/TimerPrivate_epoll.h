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

#ifndef KOURIER_TIMER_PRIVATE_H
#define KOURIER_TIMER_PRIVATE_H

#include "Timer.h"
#include "TimerListNode.h"
#include <chrono>

namespace Test::TimerNotifier {class TimerNotifierTest;}

namespace Kourier
{

class TimerWheel;
class TimerNotifier;

class TimerPrivate
{
public:
    TimerPrivate();
    ~TimerPrivate() {stop();}
    inline void start() {activateTimer(m_interval);}
    inline void start(std::chrono::milliseconds interval) {activateTimer(interval);}
    inline void stop() {deactivateTimer();}
    inline bool isActive() const {return m_state == State::Active;}
    inline bool isSingleShot() const {return m_isSingleShot || m_interval.count() == 0;}
    inline void setSingleShot(bool singleShot) {m_isSingleShot = singleShot;}
    void setInterval(std::chrono::milliseconds interval);
    inline std::chrono::milliseconds interval() const {return m_interval;}
    inline void setTimeout(std::chrono::milliseconds timeout) {m_timeout = timeout;}
    inline std::chrono::milliseconds timeout() const {return m_timeout;}
    inline void setExtraTimeout(std::chrono::milliseconds extraTimeout) {m_extraTimeout = extraTimeout;}
    inline std::chrono::milliseconds extraTimeout() const {return m_extraTimeout;}
    inline void setTimerType(Timer::TimerType timerType) {m_timerType = timerType;}
    inline Timer::TimerType timerType() const {return m_timerType;}

private:
    void activateTimer(std::chrono::milliseconds interval);
    void deactivateTimer();
    void processTimeout();

private:
    Timer *q_ptr = nullptr;
    Q_DECLARE_PUBLIC(Timer)
    TimerListNode m_listNode;
    TimerWheel *m_pTimerWheel = nullptr;
    size_t m_idxTimerWheelSlot = 0;
    std::chrono::milliseconds m_interval = std::chrono::milliseconds(0);
    std::chrono::milliseconds m_timeout = std::chrono::milliseconds(0);
    std::chrono::milliseconds m_extraTimeout = std::chrono::milliseconds(0);
    TimerNotifier *m_pTimerNotifier;
    bool m_isSingleShot = false;
    enum class State : uint8_t {Active, Inactive};
    State m_state = State::Inactive;
    Timer::TimerType m_timerType = Timer::TimerType::Coarse;
    friend class EpollTimerRegistrar;
    friend class TimerWheel;
    friend class TimerList;
    friend class TimerNotifier;
    friend class Test::TimerNotifier::TimerNotifierTest;
};

}

#endif // KOURIER_TIMER_PRIVATE_H
