/*
 * BDButton.cpp
 *
 *   SUMMARY
 *  Blue Display is an Open Source Android remote Display for Arduino etc.
 *  It receives basic draw requests from Arduino etc. over Bluetooth and renders it.
 *  It also implements basic GUI elements as buttons and sliders.
 *  GUI callback, touch and sensor events are sent back to Arduino.
 *
 *  Copyright (C) 2015  Armin Joachimsmeyer
 *  armin.joachimsmeyer@gmail.com
 *
 *  This file is part of BlueDisplay.
 *  BlueDisplay is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.

 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.

 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/gpl.html>.
 *
 */

#include "BDButton.h"
#include "BlueDisplay.h" // for BUTTONS_SET_BEEP_TONE


#ifdef LOCAL_DISPLAY_EXISTS
#include "TouchButtonAutorepeat.h"
#endif

#include <string.h>  // for strlen

/*
 * Can be interpreted as pointer to button stack.
 */
BDButtonHandle_t sLocalButtonIndex = 0;

BDButton::BDButton(void) {
}

BDButton::BDButton(BDButtonHandle_t aButtonHandle) {
    mButtonHandle = aButtonHandle;
}

#ifdef LOCAL_DISPLAY_EXISTS
BDButton::BDButton(BDButtonHandle_t aButtonHandle, TouchButton * aLocalButtonPtr) {
    mButtonHandle = aButtonHandle;
    mLocalButtonPtr = aLocalButtonPtr;
}
#endif

BDButton::BDButton(BDButton const &aButton) {
    mButtonHandle = aButton.mButtonHandle;
#ifdef LOCAL_DISPLAY_EXISTS
    mLocalButtonPtr = aButton.mLocalButtonPtr;
#endif
}

bool BDButton::operator==(const BDButton &aButton) {
#ifdef LOCAL_DISPLAY_EXISTS
    return (mButtonHandle == aButton.mButtonHandle && mLocalButtonPtr == aButton.mLocalButtonPtr);
#else
    return (mButtonHandle == aButton.mButtonHandle);
#endif
}

bool BDButton::operator!=(const BDButton &aButton) {
#ifdef LOCAL_DISPLAY_EXISTS
    return (mButtonHandle != aButton.mButtonHandle || mLocalButtonPtr != aButton.mLocalButtonPtr);
#else
    return (mButtonHandle != aButton.mButtonHandle);
#endif
}

/*
 * initialize a button stub
 * If local display is attached, allocate a button from the local pool, so do not forget to call deinit()
 */
void BDButton::init(uint16_t aPositionX, uint16_t aPositionY, uint16_t aWidthX, uint16_t aHeightY, Color_t aButtonColor,
        const char * aCaption, uint16_t aCaptionSize, uint8_t aFlags, int16_t aValue, void (*aOnTouchHandler)(BDButton*, int16_t)) {

    BDButtonHandle_t tButtonNumber = sLocalButtonIndex++;
    if (USART_isBluetoothPaired()) {
#ifndef AVR
        sendUSARTArgsAndByteBuffer(FUNCTION_BUTTON_CREATE, 11, tButtonNumber, aPositionX, aPositionY, aWidthX,
                aHeightY, aButtonColor, aCaptionSize, aFlags, aValue, aOnTouchHandler,
                (reinterpret_cast<uint32_t>(aOnTouchHandler) >> 16), strlen(aCaption), aCaption);
#else
        sendUSARTArgsAndByteBuffer(FUNCTION_BUTTON_CREATE, 10, tButtonNumber, aPositionX, aPositionY, aWidthX, aHeightY,
                aButtonColor, aCaptionSize, aFlags, aValue, aOnTouchHandler, strlen(aCaption), aCaption);
#endif
    }
    mButtonHandle = tButtonNumber;
#ifdef LOCAL_DISPLAY_EXISTS
    if (aFlags & BUTTON_FLAG_TYPE_AUTOREPEAT) {
        mLocalButtonPtr = new TouchButtonAutorepeat();
    } else {
        mLocalButtonPtr = new TouchButton();
    }
    // Cast needed here. At runtime the right pointer is returned because of FLAG_USE_BDBUTTON_FOR_CALLBACK
    mLocalButtonPtr->initButton(aPositionX, aPositionY, aWidthX, aHeightY, aButtonColor, aCaption, aCaptionSize,
            aFlags | FLAG_USE_BDBUTTON_FOR_CALLBACK, aValue,
            reinterpret_cast<void (*)(TouchButton*, int16_t)> (aOnTouchHandler));

    mLocalButtonPtr ->mBDButtonPtr = this;
#endif
}

#ifdef LOCAL_DISPLAY_EXISTS
/*
 * Assume a button stack, e.g. only local buttons are deinitialize which were initialized last.
 * localButtonIndex is used as stack pointer.
 */
void BDButton::deinit(void) {
    sLocalButtonIndex--;
    delete mLocalButtonPtr;
}
#endif

void BDButton::drawButton(void) {
#ifdef LOCAL_DISPLAY_EXISTS
    mLocalButtonPtr->drawButton();
#endif
    if (USART_isBluetoothPaired()) {
        sendUSARTArgs(FUNCTION_BUTTON_DRAW, 1, mButtonHandle);
    }
}

void BDButton::removeButton(Color_t aBackgroundColor) {
#ifdef LOCAL_DISPLAY_EXISTS
    mLocalButtonPtr->removeButton(aBackgroundColor);
#endif
    if (USART_isBluetoothPaired()) {
        sendUSARTArgs(FUNCTION_BUTTON_REMOVE, 2, mButtonHandle, aBackgroundColor);
    }
}

void BDButton::drawCaption(void) {
#ifdef LOCAL_DISPLAY_EXISTS
    mLocalButtonPtr->drawCaption();
#endif
    if (USART_isBluetoothPaired()) {
        sendUSARTArgs(FUNCTION_BUTTON_DRAW_CAPTION, 1, mButtonHandle);
    }
}

void BDButton::setCaption(const char * aCaption) {
#ifdef LOCAL_DISPLAY_EXISTS
    mLocalButtonPtr->setCaption(aCaption);
#endif
    if (USART_isBluetoothPaired()) {
        sendUSARTArgsAndByteBuffer(FUNCTION_BUTTON_SET_CAPTION, 1, mButtonHandle, strlen(aCaption), aCaption);
    }
}

void BDButton::setCaptionAndDraw(const char * aCaption) {
#ifdef LOCAL_DISPLAY_EXISTS
    mLocalButtonPtr->setCaption(aCaption);
    mLocalButtonPtr->drawButton();
#endif
    if (USART_isBluetoothPaired()) {
        sendUSARTArgsAndByteBuffer(FUNCTION_BUTTON_SET_CAPTION_AND_DRAW_BUTTON, 1, mButtonHandle, strlen(aCaption), aCaption);
    }
}

void BDButton::setCaption(const char * aCaption, bool doDrawButton) {
#ifdef LOCAL_DISPLAY_EXISTS
    mLocalButtonPtr->setCaption(aCaption);
    if (doDrawButton) {
        mLocalButtonPtr->drawButton();
    }
#endif
    if (USART_isBluetoothPaired()) {
        uint8_t tFunctionCode = FUNCTION_BUTTON_SET_CAPTION;
        if (doDrawButton) {
            tFunctionCode = FUNCTION_BUTTON_SET_CAPTION_AND_DRAW_BUTTON;
        }
        sendUSARTArgsAndByteBuffer(tFunctionCode, 1, mButtonHandle, strlen(aCaption), aCaption);
    }
}

void BDButton::setValue(int16_t aValue) {
#ifdef LOCAL_DISPLAY_EXISTS
    mLocalButtonPtr->setValue(aValue);
#endif
    if (USART_isBluetoothPaired()) {
        sendUSARTArgs(FUNCTION_BUTTON_SETTINGS, 3, mButtonHandle, SUBFUNCTION_BUTTON_SET_VALUE, aValue);
    }
}

void BDButton::setValueAndDraw(int16_t aValue) {
#ifdef LOCAL_DISPLAY_EXISTS
    mLocalButtonPtr->setValue(aValue);
    mLocalButtonPtr->drawButton();
#endif
    if (USART_isBluetoothPaired()) {
        sendUSARTArgs(FUNCTION_BUTTON_SETTINGS, 3, mButtonHandle, SUBFUNCTION_BUTTON_SET_VALUE_AND_DRAW, aValue);
    }
}

void BDButton::setButtonColor(Color_t aButtonColor) {
#ifdef LOCAL_DISPLAY_EXISTS
    mLocalButtonPtr->setButtonColor(aButtonColor);
#endif
    if (USART_isBluetoothPaired()) {
        sendUSARTArgs(FUNCTION_BUTTON_SETTINGS, 3, mButtonHandle, SUBFUNCTION_BUTTON_SET_BUTTON_COLOR, aButtonColor);
    }
}

void BDButton::setButtonColorAndDraw(Color_t aButtonColor) {
#ifdef LOCAL_DISPLAY_EXISTS
    mLocalButtonPtr->setButtonColor(aButtonColor);
    mLocalButtonPtr->drawButton();
#endif
    if (USART_isBluetoothPaired()) {
        sendUSARTArgs(FUNCTION_BUTTON_SETTINGS, 3, mButtonHandle, SUBFUNCTION_BUTTON_SET_BUTTON_COLOR_AND_DRAW, aButtonColor);
    }
}

void BDButton::setPosition(int16_t aPositionX, int16_t aPositionY) {
#ifdef LOCAL_DISPLAY_EXISTS
    mLocalButtonPtr->setPosition(aPositionX, aPositionY);
#endif
    if (USART_isBluetoothPaired()) {
        sendUSARTArgs(FUNCTION_BUTTON_SETTINGS, 4, mButtonHandle, SUBFUNCTION_BUTTON_SET_POSITION, aPositionX, aPositionY);
    }
}

/**
 * after aMillisFirstDelay milliseconds a callback is done every aMillisFirstRate milliseconds for aFirstCount times
 * after this a callback is done every aMillisSecondRate milliseconds
 */
void BDButton::setButtonAutorepeatTiming(uint16_t aMillisFirstDelay, uint16_t aMillisFirstRate, uint16_t aFirstCount,
        uint16_t aMillisSecondRate) {
#ifdef LOCAL_DISPLAY_EXISTS
    ((TouchButtonAutorepeat*) mLocalButtonPtr)->setButtonAutorepeatTiming(aMillisFirstDelay, aMillisFirstRate,
            aFirstCount, aMillisSecondRate);
#endif
    if (USART_isBluetoothPaired()) {
        sendUSARTArgs(FUNCTION_BUTTON_SETTINGS, 7, mButtonHandle, SUBFUNCTION_BUTTON_SET_AUTOREPEAT_TIMING, aMillisFirstDelay,
                aMillisFirstRate, aFirstCount, aMillisSecondRate);
    }
}

void BDButton::activate(void) {
#ifdef LOCAL_DISPLAY_EXISTS
    mLocalButtonPtr->activate();
#endif
    if (USART_isBluetoothPaired()) {
        sendUSARTArgs(FUNCTION_BUTTON_SETTINGS, 2, mButtonHandle, SUBFUNCTION_BUTTON_SET_ACTIVE);
    }
}

void BDButton::deactivate(void) {
#ifdef LOCAL_DISPLAY_EXISTS
    mLocalButtonPtr->deactivate();
#endif
    if (USART_isBluetoothPaired()) {
        sendUSARTArgs(FUNCTION_BUTTON_SETTINGS, 2, mButtonHandle, SUBFUNCTION_BUTTON_RESET_ACTIVE);
    }
}

/*
 * Static functions
 */
void BDButton::resetAllButtons(void) {
    sLocalButtonIndex = 0;
}

void BDButton::setGlobalFlags(uint16_t aFlags) {
    if (USART_isBluetoothPaired()) {
        sendUSARTArgs(FUNCTION_BUTTON_GLOBAL_SETTINGS, 1, aFlags);
    }
}

/*
 * aToneVolume: value in percent
 */
void BDButton::setButtonsTouchTone(uint8_t aToneIndex, uint16_t aToneDuration) {
    if (USART_isBluetoothPaired()) {
        sendUSARTArgs(FUNCTION_BUTTON_GLOBAL_SETTINGS, 3, BUTTONS_SET_BEEP_TONE, aToneIndex, aToneDuration);
    }
}

void BDButton::setButtonsTouchTone(uint8_t aToneIndex, uint16_t aToneDuration, uint8_t aToneVolume) {
    if (USART_isBluetoothPaired()) {
        sendUSARTArgs(FUNCTION_BUTTON_GLOBAL_SETTINGS, 4, BUTTONS_SET_BEEP_TONE, aToneIndex, aToneDuration, aToneVolume);
    }
}

void BDButton::activateAllButtons(void) {
    if (USART_isBluetoothPaired()) {
        sendUSARTArgs(FUNCTION_BUTTON_ACTIVATE_ALL, 0);
    }
}

void BDButton::deactivateAllButtons(void) {
#ifdef LOCAL_DISPLAY_EXISTS
    TouchButton::deactivateAllButtons();
#endif
    if (USART_isBluetoothPaired()) {
        sendUSARTArgs(FUNCTION_BUTTON_DEACTIVATE_ALL, 0);
    }
}

/**
 *
 * @param aTheTouchedButton
 * @param aValue assume as boolean here
 */
void doToggleRedGreenButton(BDButton * aTheTouchedButton, int16_t aValue) {
    aValue = !aValue;
    aTheTouchedButton->setValueAndDraw(aValue);
}

#ifdef AVR
void BDButton::initPGM(uint16_t aPositionX, uint16_t aPositionY, uint16_t aWidthX, uint16_t aHeightY, Color_t aButtonColor,
        const char * aPGMCaption, uint8_t aCaptionSize, uint8_t aFlags, int16_t aValue,
        void (*aOnTouchHandler)(BDButton*, int16_t)) {

    BDButtonHandle_t tButtonNumber = sLocalButtonIndex++;
    if (USART_isBluetoothPaired()) {
        uint8_t tCaptionLength = strlen_P(aPGMCaption);
        if (tCaptionLength < STRING_BUFFER_STACK_SIZE) {
            char StringBuffer[STRING_BUFFER_STACK_SIZE];
            strcpy_P(StringBuffer, aPGMCaption);
            sendUSARTArgsAndByteBuffer(FUNCTION_BUTTON_CREATE, 10, tButtonNumber, aPositionX, aPositionY, aWidthX, aHeightY,
                    aButtonColor, aCaptionSize, aFlags, aValue, aOnTouchHandler, tCaptionLength, StringBuffer);
        }
    }
    mButtonHandle = tButtonNumber;
}

void BDButton::setCaptionPGM(const char * aPGMCaption) {
    if (USART_isBluetoothPaired()) {
        uint8_t tCaptionLength = strlen_P(aPGMCaption);
        if (tCaptionLength < STRING_BUFFER_STACK_SIZE) {
            char StringBuffer[STRING_BUFFER_STACK_SIZE];
            strcpy_P(StringBuffer, aPGMCaption);
            sendUSARTArgsAndByteBuffer(FUNCTION_BUTTON_SET_CAPTION, 1, mButtonHandle, tCaptionLength, StringBuffer);
        }
    }
}

void BDButton::setCaptionPGM(const char * aPGMCaption, bool doDrawButton) {
    if (USART_isBluetoothPaired()) {
        uint8_t tCaptionLength = strlen_P(aPGMCaption);
        if (tCaptionLength < STRING_BUFFER_STACK_SIZE) {
            char StringBuffer[STRING_BUFFER_STACK_SIZE];
            strcpy_P(StringBuffer, aPGMCaption);
            uint8_t tFunctionCode = FUNCTION_BUTTON_SET_CAPTION;
            if (doDrawButton) {
                tFunctionCode = FUNCTION_BUTTON_SET_CAPTION_AND_DRAW_BUTTON;
            }
            sendUSARTArgsAndByteBuffer(tFunctionCode, 1, mButtonHandle, tCaptionLength, StringBuffer);
        }
    }
}
#endif
