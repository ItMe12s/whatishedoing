#pragma once

#include <string>

class GJGameLevel;

namespace level_upload {

std::string buildUploadMessage(GJGameLevel* level, bool isUpdate);

} // namespace level_upload
