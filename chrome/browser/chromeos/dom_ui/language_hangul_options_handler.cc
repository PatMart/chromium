// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/dom_ui/language_hangul_options_handler.h"

#include "app/l10n_util.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/chromeos/language_preferences.h"
#include "grit/generated_resources.h"

namespace chromeos {

LanguageHangulOptionsHandler::LanguageHangulOptionsHandler() {
}

LanguageHangulOptionsHandler::~LanguageHangulOptionsHandler() {
}

void LanguageHangulOptionsHandler::GetLocalizedValues(
    DictionaryValue* localized_strings) {
  DCHECK(localized_strings);
  // Language Hangul page - ChromeOS
  localized_strings->SetString("keyboard_layout",
      l10n_util::GetStringUTF16(IDS_OPTIONS_SETTINGS_KEYBOARD_LAYOUT_TEXT));

  localized_strings->Set("keyboardLayoutList", GetKeyboardLayoutList());
}

ListValue* LanguageHangulOptionsHandler::GetKeyboardLayoutList() {
  ListValue* keyboard_layout_list = new ListValue();
  for (size_t i = 0; i < arraysize(kHangulKeyboardNameIDPairs); ++i) {
    ListValue* option = new ListValue();
    option->Append(Value::CreateStringValue(
        kHangulKeyboardNameIDPairs[i].keyboard_id));
    option->Append(Value::CreateStringValue(l10n_util::GetStringUTF16(
        kHangulKeyboardNameIDPairs[i].message_id)));
    keyboard_layout_list->Append(option);
  }
  return keyboard_layout_list;
}

}  // namespace chromeos
