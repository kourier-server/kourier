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

#ifndef KOURIER_TIMER_NOTIFIER_H
#define KOURIER_TIMER_NOTIFIER_H

#include "EpollEventSource.h"
#include "TimerWheel.h"
#include "ClockTicker.h"
#include <memory>

namespace Test::TimerNotifier {class TimerNotifierTest;}

namespace Kourier
{

class TimerNotifier : public EpollEventSource
{
KOURIER_OBJECT(Kourier::TimerNotifier)
public:
    TimerNotifier();
    TimerNotifier(std::shared_ptr<ClockTicker> pLowResolutionClockTicker,
                  std::shared_ptr<ClockTicker> pHighResolutionClockTicker);
    ~TimerNotifier() override;
    void addTimer(TimerPrivate *pTimer);
    void removeTimer(TimerPrivate *pTimer);
    inline std::chrono::milliseconds lowResolutionTime() const {return m_lowResolutionTime;}
    int64_t fileDescriptor() const override {return m_eventFd;}
    inline static std::chrono::milliseconds msSinceEpoch() {return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch());}
    inline uint64_t timerCount(size_t wheelIdx) const
    {
        if (wheelIdx < 7) [[likely]]
            return m_timerWheels[wheelIdx].timerCount();
        else [[unlikely]]
            return 0;
    }

private:
    void addAdjustedTimer(TimerPrivate *pTimer);
    void onLowResolutionTick();
    void onHighResolutionTick();
    inline void setHighResolutionClockTickerEnabled(bool enabled) {m_pHighResolutionClockTicker->setEnabled(enabled);}
    inline void processExpiredTimers(TimerList &expiredTimers)
    {
        while (!expiredTimers.isEmpty())
        {
            auto *pTimer = expiredTimers.popFirst();
            pTimer->setTimeout(pTimer->timeout() + pTimer->extraTimeout());
            pTimer->setExtraTimeout(std::chrono::milliseconds(0));
            addAdjustedTimer(pTimer);
        }
    }
    void set();
    void reset();
    inline void triggerTimerWhenControlReturnsToEventLoop(TimerPrivate *pTimer)
    {
        assert(pTimer);
        set();
        pTimer->m_pTimerWheel = nullptr;
        pTimer->m_idxTimerWheelSlot = 0;
        m_timersToNotify.pushFront(pTimer);
    }
    void onEvent(uint32_t epollEvents) override;
    void notifyTimers();
    void disable();

private:
    static constexpr uint64_t maxTimeout = 1ull << 42;
    std::chrono::milliseconds m_lowResolutionTime = std::chrono::milliseconds(0);
    uint64_t m_lowResolutionTickCounter = 0;
    TimerList m_timersToNotify;
    std::shared_ptr<ClockTicker> m_pLowResolutionClockTicker;
    std::shared_ptr<ClockTicker> m_pHighResolutionClockTicker;
    TimerWheel m_timerWheels[7] = {TimerWheel(std::chrono::milliseconds(1)),
                                   TimerWheel(std::chrono::milliseconds(1ull << 6)),
                                   TimerWheel(std::chrono::milliseconds(1ull << 12)),
                                   TimerWheel(std::chrono::milliseconds(1ull << 18)),
                                   TimerWheel(std::chrono::milliseconds(1ull << 24)),
                                   TimerWheel(std::chrono::milliseconds(1ull << 30)),
                                   TimerWheel(std::chrono::milliseconds(1ull << 36))};
    int64_t m_eventFd = -1;
    bool m_eventIsSet = false;
    bool m_isNotifyingTimers = false;
    bool m_isEnabled = true;
    friend class Test::TimerNotifier::TimerNotifierTest;
    friend class TimerNotifierDisabler;
};

}

#endif // KOURIER_TIMER_NOTIFIER_H
