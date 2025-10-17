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

#ifndef KOURIER_CLOCK_TICKER_H
#define KOURIER_CLOCK_TICKER_H

#include "EpollEventSource.h"
#include <chrono>


namespace Kourier
{

class ClockTicker : public EpollEventSource
{
KOURIER_OBJECT(Kourier::ClockTicker)
public:
    ClockTicker(std::chrono::milliseconds resolution);
    ~ClockTicker() override;
    Signal tick();
    void setEnabled(bool enabled);
    inline bool isEnabled() const {return m_enabled;}
    inline std::chrono::milliseconds currentTime() const {return m_currentTime;}
    int64_t fileDescriptor() const override {return m_timerFd;}

private:
    void onEvent(uint32_t epollEvents) override;
    void activateTicker();
    void deactivateTicker();

private:
    const std::chrono::milliseconds m_resolution = std::chrono::milliseconds(0);
    std::chrono::milliseconds m_currentTime = std::chrono::milliseconds(0);
    const int64_t m_timerFd;
    bool m_enabled = false;
};

}

#endif // KOURIER_CLOCK_TICKER_H
