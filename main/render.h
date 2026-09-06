#pragma once
#include "dashboard.h"
#include "zectrix_canvas.h"
namespace home {
void RenderDashboard(ZectrixCanvas&, const Dashboard&, int page, time_t now, bool online, int battery_percent = -1);
void RenderMessage(ZectrixCanvas&, const char* title, const std::vector<std::string>& lines);
int SectionStart(const Dashboard&, int section);
int SectionPages(const Dashboard&, int section);
int PageCount(const Dashboard&);
}
