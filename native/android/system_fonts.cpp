/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <jni.h>

#include <android/system_fonts.h>

#include <memory>
#include <string>
#include <vector>

#include <errno.h>
#include <fcntl.h>
#include <libxml/tree.h>
#include <log/log.h>
#include <sys/stat.h>
#include <unistd.h>

struct XmlCharDeleter {
    void operator()(xmlChar* b) { xmlFree(b); }
};

struct XmlDocDeleter {
    void operator()(xmlDoc* d) { xmlFreeDoc(d); }
};

using XmlCharUniquePtr = std::unique_ptr<xmlChar, XmlCharDeleter>;
using XmlDocUniquePtr = std::unique_ptr<xmlDoc, XmlDocDeleter>;

struct ASystemFontIterator {
    XmlDocUniquePtr mXmlDoc;
    xmlNode* mFontNode;
};

struct ASystemFont {
    std::string mFilePath;
    std::unique_ptr<std::string> mLocale;
    uint16_t mWeight;
    bool mItalic;
    uint32_t mCollectionIndex;
    std::vector<std::pair<uint32_t, float>> mAxes;
};

namespace {

std::string xmlTrim(const std::string& in) {
    if (in.empty()) {
        return in;
    }
    const char XML_SPACES[] = "\u0020\u000D\u000A\u0009";
    const size_t start = in.find_first_not_of(XML_SPACES);  // inclusive
    if (start == std::string::npos) {
        return "";
    }
    const size_t end = in.find_last_not_of(XML_SPACES);     // inclusive
    if (end == std::string::npos) {
        return "";
    }
    return in.substr(start, end - start + 1 /* +1 since end is inclusive */);
}

const xmlChar* FAMILY_TAG = BAD_CAST("family");
const xmlChar* FONT_TAG = BAD_CAST("font");

xmlNode* firstElement(xmlNode* node, const xmlChar* tag) {
    for (xmlNode* child = node->children; child; child = child->next) {
        if (xmlStrEqual(child->name, tag)) {
            return child;
        }
    }
    return nullptr;
}

xmlNode* nextSibling(xmlNode* node, const xmlChar* tag) {
    while ((node = node->next) != nullptr) {
        if (xmlStrEqual(node->name, tag)) {
            return node;
        }
    }
    return nullptr;
}

void copyFont(ASystemFontIterator* ite, ASystemFont* out) {
    const xmlChar* LOCALE_ATTR_NAME = BAD_CAST("lang");
    XmlCharUniquePtr filePathStr(
            xmlNodeListGetString(ite->mXmlDoc.get(), ite->mFontNode->xmlChildrenNode, 1));
    out->mFilePath = "/system/fonts/" + xmlTrim(
            std::string(filePathStr.get(), filePathStr.get() + xmlStrlen(filePathStr.get())));

    const xmlChar* WEIGHT_ATTR_NAME = BAD_CAST("weight");
    XmlCharUniquePtr weightStr(xmlGetProp(ite->mFontNode, WEIGHT_ATTR_NAME));
    out->mWeight = weightStr ?
            strtol(reinterpret_cast<const char*>(weightStr.get()), nullptr, 10) : 400;

    const xmlChar* STYLE_ATTR_NAME = BAD_CAST("style");
    const xmlChar* ITALIC_ATTR_VALUE = BAD_CAST("italic");
    XmlCharUniquePtr styleStr(xmlGetProp(ite->mFontNode, STYLE_ATTR_NAME));
    out->mItalic = styleStr ? xmlStrEqual(styleStr.get(), ITALIC_ATTR_VALUE) : false;

    const xmlChar* INDEX_ATTR_NAME = BAD_CAST("index");
    XmlCharUniquePtr indexStr(xmlGetProp(ite->mFontNode, INDEX_ATTR_NAME));
    out->mCollectionIndex =  indexStr ?
            strtol(reinterpret_cast<const char*>(indexStr.get()), nullptr, 10) : 0;

    XmlCharUniquePtr localeStr(xmlGetProp(ite->mXmlDoc->parent, LOCALE_ATTR_NAME));
    out->mLocale.reset(
            localeStr ? new std::string(reinterpret_cast<const char*>(localeStr.get())) : nullptr);

    const xmlChar* TAG_ATTR_NAME = BAD_CAST("tag");
    const xmlChar* STYLEVALUE_ATTR_NAME = BAD_CAST("stylevalue");
    const xmlChar* AXIS_TAG = BAD_CAST("axis");
    out->mAxes.clear();
    for (xmlNode* axis = firstElement(ite->mFontNode, AXIS_TAG); axis;
            axis = nextSibling(axis, AXIS_TAG)) {
        XmlCharUniquePtr tagStr(xmlGetProp(axis, TAG_ATTR_NAME));
        if (!tagStr || xmlStrlen(tagStr.get()) != 4) {
            continue;  // Tag value must be 4 char string
        }

        XmlCharUniquePtr styleValueStr(xmlGetProp(axis, STYLEVALUE_ATTR_NAME));
        if (!styleValueStr) {
            continue;
        }

        uint32_t tag =
            static_cast<uint32_t>(tagStr.get()[0] << 24) |
            static_cast<uint32_t>(tagStr.get()[1] << 16) |
            static_cast<uint32_t>(tagStr.get()[2] << 8) |
            static_cast<uint32_t>(tagStr.get()[3]);
        float styleValue = strtod(reinterpret_cast<const char*>(styleValueStr.get()), nullptr);
        out->mAxes.push_back(std::make_pair(tag, styleValue));
    }
}

bool isFontFileAvailable(const std::string& filePath) {
    std::string fullPath = filePath;
    struct stat st = {};
    if (stat(fullPath.c_str(), &st) != 0) {
        return false;
    }
    return S_ISREG(st.st_mode);
}

xmlNode* findFirstFontNode(xmlDoc* doc) {
    xmlNode* familySet = xmlDocGetRootElement(doc);
    if (familySet == nullptr) {
        return nullptr;
    }
    xmlNode* family = firstElement(familySet, FAMILY_TAG);
    if (family == nullptr) {
        return nullptr;
    }

    xmlNode* font = firstElement(family, FONT_TAG);
    while (font == nullptr) {
        family = nextSibling(family, FAMILY_TAG);
        if (family == nullptr) {
            return nullptr;
        }
        font = firstElement(family, FONT_TAG);
    }
    return font;
}

}  // namespace

ASystemFontIterator* ASystemFontIterator_open() {
    std::unique_ptr<ASystemFontIterator> ite(new ASystemFontIterator());
    ite->mXmlDoc.reset(xmlReadFile("/system/etc/fonts.xml", nullptr, 0));
    return ite.release();
}

void ASystemFontIterator_close(ASystemFontIterator* ite) {
    delete ite;
}

ASystemFont* ASystemFontIterator_next(ASystemFontIterator* ite) {
    LOG_ALWAYS_FATAL_IF(ite == nullptr, "nullptr has passed as iterator argument");
    if (ite->mFontNode == nullptr) {
        if (ite->mXmlDoc == nullptr) {
            return nullptr;  // Already at the end.
        } else {
            // First time to query font.
            ite->mFontNode = findFirstFontNode(ite->mXmlDoc.get());
            if (ite->mFontNode == nullptr) {
                ite->mXmlDoc.reset();
                return nullptr;  // No font node found.
            }
            std::unique_ptr<ASystemFont> font = std::make_unique<ASystemFont>();
            copyFont(ite, font.get());
            return font.release();
        }
    } else {
        xmlNode* nextNode = nextSibling(ite->mFontNode, FONT_TAG);
        while (nextNode == nullptr) {
            xmlNode* family = nextSibling(ite->mFontNode->parent, FAMILY_TAG);
            if (family == nullptr) {
                break;
            }
            nextNode = firstElement(family, FONT_TAG);
        }
        ite->mFontNode = nextNode;
        if (nextNode == nullptr) {
            ite->mXmlDoc.reset();
            return nullptr;
        }

        std::unique_ptr<ASystemFont> font = std::make_unique<ASystemFont>();
        copyFont(ite, font.get());
        if (!isFontFileAvailable(font->mFilePath)) {
            // fonts.xml intentionally contains missing font configuration. Skip it.
            return ASystemFontIterator_next(ite);
        }
        return font.release();
    }
}

void ASystemFont_close(ASystemFont* font) {
    delete font;
}

const char* ASystemFont_getFontFilePath(const ASystemFont* font) {
    LOG_ALWAYS_FATAL_IF(font == nullptr, "nullptr has passed as font argument");
    return font->mFilePath.c_str();
}

uint16_t ASystemFont_getWeight(const ASystemFont* font) {
    LOG_ALWAYS_FATAL_IF(font == nullptr, "nullptr has passed as font argument");
    return font->mWeight;
}

bool ASystemFont_isItalic(const ASystemFont* font) {
    LOG_ALWAYS_FATAL_IF(font == nullptr, "nullptr has passed as font argument");
    return font->mItalic;
}

const char* ASystemFont_getLocale(const ASystemFont* font) {
    LOG_ALWAYS_FATAL_IF(font == nullptr, "nullptr has passed to font argument");
    return font->mLocale ? nullptr : font->mLocale->c_str();
}

size_t ASystemFont_getCollectionIndex(const ASystemFont* font) {
    LOG_ALWAYS_FATAL_IF(font == nullptr, "nullptr has passed to font argument");
    return font->mCollectionIndex;
}

size_t ASystemFont_getAxisCount(const ASystemFont* font) {
    LOG_ALWAYS_FATAL_IF(font == nullptr, "nullptr has passed to font argument");
    return font->mAxes.size();
}

uint32_t ASystemFont_getAxisTag(const ASystemFont* font, uint32_t axisIndex) {
    LOG_ALWAYS_FATAL_IF(font == nullptr, "nullptr has passed to font argument");
    LOG_ALWAYS_FATAL_IF(axisIndex >= font->mAxes.size(),
                        "given axis index is out of bounds. (< %zd", font->mAxes.size());
    return font->mAxes[axisIndex].first;
}

float ASystemFont_getAxisValue(const ASystemFont* font, uint32_t axisIndex) {
    LOG_ALWAYS_FATAL_IF(font == nullptr, "nullptr has passed to font argument");
    LOG_ALWAYS_FATAL_IF(axisIndex >= font->mAxes.size(),
                        "given axis index is out of bounds. (< %zd", font->mAxes.size());
    return font->mAxes[axisIndex].second;
}
