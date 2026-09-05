#pragma once
#include "dashboard.h"
#include "zectrix_canvas.h"
namespace home {
void RenderDashboard(ZectrixCanvas&, const Dashboard&, int page, time_t now, bool online);
void RenderMessage(ZectrixCanvas&, const char* title, const std::vector<std::string>& lines);
int PageCount(const Dashboard&);
}
