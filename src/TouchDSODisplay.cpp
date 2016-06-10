/**
 * TouchDSODisplay.cpp
 * @brief contains the display related functions for DSO.
 *
 * @date 29.03.2012
 * @author Armin Joachimsmeyer
 * armin.joachimsmeyer@gmail.com
 * @copyright LGPL v3 (http://www.gnu.org/licenses/lgpl.html)
 * @version 1.5.0
 *
 */

#include "Pages.h"
#include "TouchDSO.h"
#include "utils.h" // for formatThousandSeparator()

#include "BlueSerial.h"
#include <string.h>
#include "Chart.h" // for drawChartDataFloat()

/*****************************
 * Display stuff
 *****************************/
uint8_t DisplayBufferFFT[FFT_SIZE / 2];
uint8_t DisplayBuffer[DSO_DISPLAY_WIDTH]; // Buffer for raw display data of current chart (maximum values)
uint8_t DisplayBufferMin[DSO_DISPLAY_WIDTH]; // Buffer for raw display data of current chart minimum values
uint8_t DisplayBuffer2[DSO_DISPLAY_WIDTH]; // Buffer for trigger state line

/*
 * Display control
 * while running switch between upper info line on/off
 * while stopped switch between chart / t+info line and gui
 */

DisplayControlStruct DisplayControl;

const char * const TriggerStatusStrings[] = { "slope", "level", "nothing" }; // waiting for slope, waiting for trigger level (slope condition is met)

/*
 * FFT
 */
Chart ChartFFT;
#define FFT_DISPLAY_SCALE_FACTOR_X (BlueDisplay1.getDisplayWidth() / FFT_SIZE) // 2 pixel per value
/*******************************************************************************************
 * Program code starts here
 *******************************************************************************************/

/************************************************************************
 * Graphical output section
 ************************************************************************/

void clearTriggerLine(uint8_t aTriggerLevelDisplayValue) {
    // clear old line
    BlueDisplay1.drawLineRel(0, aTriggerLevelDisplayValue, DSO_DISPLAY_WIDTH, 0, COLOR_BACKGROUND_DSO);
    // restore grid at old y position
    for (int tXPos = TIMING_GRID_WIDTH - 1; tXPos < DSO_DISPLAY_WIDTH - 1; tXPos += TIMING_GRID_WIDTH) {
        BlueDisplay1.drawPixel(tXPos, aTriggerLevelDisplayValue, COLOR_GRID_LINES);
    }
    if (!MeasurementControl.isRunning) {
        // in analysis mode restore graph at old y position
        uint8_t* ScreenBufferPointer = &DisplayBuffer[0];
        for (int i = 0; i < DSO_DISPLAY_WIDTH; ++i) {
            int tValueByte = *ScreenBufferPointer++;
            if (tValueByte == aTriggerLevelDisplayValue) {
                // restore old pixel
                BlueDisplay1.drawPixel(i, tValueByte, COLOR_DATA_HOLD);
            }
        }
    }
}

/**
 * draws trigger line if it is visible - do not draw clipped value e.g. value was higher than display range
 */
void drawTriggerLine(void) {
    if (DisplayControl.TriggerLevelDisplayValue != 0 && MeasurementControl.TriggerMode != TRIGGER_MODE_OFF) {
        BlueDisplay1.drawLineRel(0, DisplayControl.TriggerLevelDisplayValue, DSO_DISPLAY_WIDTH, 0,
        COLOR_TRIGGER_LINE);
    }
}

/**
 * draws vertical timing + horizontal reference voltage lines
 */
void drawGridLinesWithHorizLabelsAndTriggerLine(Color_t aColor) {
    //vertical lines
    for (int tXPos = TIMING_GRID_WIDTH - 1; tXPos < DSO_DISPLAY_WIDTH; tXPos += TIMING_GRID_WIDTH) {
        BlueDisplay1.drawLineRel(tXPos, 0, 0, DSO_DISPLAY_HEIGHT, aColor);
    }
    // add 0.0001 to avoid display of -0.00
    float tActualVoltage = (ScaleVoltagePerDiv[MeasurementControl.DisplayRangeIndexForPrint]
            * (MeasurementControl.OffsetGridCount) + 0.0001);
    bool tLabelChanged = false;
    if (DisplayControl.LastDisplayRangeIndex != MeasurementControl.DisplayRangeIndexForPrint
            || DisplayControl.LastOffsetGridCount != MeasurementControl.OffsetGridCount) {
        DisplayControl.LastDisplayRangeIndex = MeasurementControl.DisplayRangeIndexForPrint;
        DisplayControl.LastOffsetGridCount = MeasurementControl.OffsetGridCount;
        tLabelChanged = true;
    }
    int tCaptionOffset = 1;
    Color_t tLabelColor;
    int tPosX;
    for (int tYPos = DISPLAY_VALUE_FOR_ZERO; tYPos > 0; tYPos -= HORIZONTAL_GRID_HEIGHT) {
        if (tLabelChanged) {
            // clear old label
            int tXpos = DSO_DISPLAY_WIDTH - PIXEL_AFTER_LABEL - (5 * TEXT_SIZE_11_WIDTH);
            int tY = tYPos - tCaptionOffset;
            BlueDisplay1.fillRect(tXpos, tY - TEXT_SIZE_11_ASCEND, DSO_DISPLAY_WIDTH - PIXEL_AFTER_LABEL + 1,
                    tY + TEXT_SIZE_11_HEIGHT - TEXT_SIZE_11_ASCEND, COLOR_BACKGROUND_DSO);
            // restore vertical line
            BlueDisplay1.drawLineRel(9 * TIMING_GRID_WIDTH - 1, tY, 0, TEXT_SIZE_11_HEIGHT, aColor);
        }
        // draw horizontal line
        BlueDisplay1.drawLineRel(0, tYPos, DSO_DISPLAY_WIDTH, 0, aColor);

        int tCount = snprintf(StringBuffer, sizeof StringBuffer, "%0.*f",
                RangePrecision[MeasurementControl.DisplayRangeIndexForPrint], tActualVoltage);
        // right align but leave 2 pixel free after label for the last horizontal line
        tPosX = DSO_DISPLAY_WIDTH - (tCount * TEXT_SIZE_11_WIDTH) - PIXEL_AFTER_LABEL;
        // draw label over the line - use different color for negative values
        if (tActualVoltage >= 0) {
            tLabelColor = COLOR_HOR_GRID_LINE_LABEL;
        } else {
            tLabelColor = COLOR_HOR_GRID_LINE_LABEL_NEGATIVE;
        }
        BlueDisplay1.drawText(tPosX, tYPos - tCaptionOffset, StringBuffer, TEXT_SIZE_11, tLabelColor,
        COLOR_BACKGROUND_DSO);
        tCaptionOffset = -(TEXT_SIZE_11_ASCEND / 2);
        tActualVoltage += ScaleVoltagePerDiv[MeasurementControl.DisplayRangeIndexForPrint];
    }
    drawTriggerLine();
}

/*
 * draws min, max lines
 */
void drawMinMaxLines(void) {
// draw max line
    int tValueDisplay = getDisplayFrowRawInputValue(MeasurementControl.RawValueMax);
    if (tValueDisplay != 0) {
        BlueDisplay1.drawLineRel(0, tValueDisplay, DSO_DISPLAY_WIDTH, 0, COLOR_MAX_MIN_LINE);
    }
// min line
    tValueDisplay = getDisplayFrowRawInputValue(MeasurementControl.RawValueMin);
    if (tValueDisplay != DISPLAY_VALUE_FOR_ZERO) {
        BlueDisplay1.drawLineRel(0, tValueDisplay, DSO_DISPLAY_WIDTH, 0, COLOR_MAX_MIN_LINE);
    }
}

/**
 * Draws data on screen
 * @param aDataBufferPointer Data is taken from DataBufferPointer.
 * @param aLength
 * @param aColor Draw color. - used for different colors for pretrigger data or different modes
 * @param aClearBeforeColor if > 0 data from DisplayBuffer is drawn(erased) with this color
 *              just before to avoid interfering with display refresh timing. - Color is used for history modes.
 *              DataBufferPointer must not be null then!
 * @param aDrawMode DRAW_MODE_REGULAR, DRAW_MODE_CLEAR_OLD, DRAW_MODE_CLEAR_OLD_MIN
 * @param aDrawAlsoMin equal to MeasurementControl.isEffectiveMinMaxMode except for singleshot preview
 * @note if aClearBeforeColor > 0 then DataBufferPointer must not be NULL
 * @note if isEffectiveMinMaxMode == true then DisplayBufferMin is processed subsequently
 * @note NOT used for drawing while acquiring
 */
void drawDataBuffer(uint16_t *aDataBufferPointer, int aLength, Color_t aColor, Color_t aClearBeforeColor,
        int aDrawMode, bool aDrawAlsoMin) {
    int i;
    int tValue;
#ifdef LOCAL_DISPLAY_EXISTS
    int tCounterForTimingGridRestore = 0;
    int tLastValue = 0; // to avoid compiler warnings
    int tLastValueClear = 0; // to avoid compiler warnings
#endif

    uint16_t *tDataBufferPointer = aDataBufferPointer;
    uint8_t *ScreenBufferReadPointer;
    uint8_t *ScreenBufferWritePointer1;
    bool tProcessMaxValues;
    if (aDrawMode == DRAW_MODE_CLEAR_OLD_MIN) {
        ScreenBufferReadPointer = &DisplayBufferMin[0];
        ScreenBufferWritePointer1 = &DisplayBufferMin[0];
        tProcessMaxValues = false;
    } else {
        ScreenBufferReadPointer = &DisplayBuffer[0];
        ScreenBufferWritePointer1 = &DisplayBuffer[0];
        tProcessMaxValues = true;
    }
    uint8_t *ScreenBufferWritePointer2 = &DisplayBuffer2[0]; // for trigger state line
    int tXScale = DisplayControl.XScale;
    int tXScaleCounter = tXScale;
    int tTriggerValue = getDisplayFrowRawInputValue(MeasurementControl.RawTriggerLevel);

    do {
        if (tXScale <= 0) {
            tXScaleCounter = -tXScale;
        }
        for (i = 0; i < aLength; ++i) {
            if (aDrawMode != DRAW_MODE_REGULAR) {
                // get data from screen buffer in order to erase it
                tValue = *ScreenBufferReadPointer;
            } else {
                tValue = getDisplayFrowRawInputValue(*tDataBufferPointer);
                /*
                 * get data from data buffer and perform X scaling
                 */
                if (tXScale == 0) {
                    tDataBufferPointer++;
                } else if (tXScale < -1) {
                    // compress - get average of multiple values
                    tValue = getDisplayFrowMultipleRawValues(tDataBufferPointer, tXScaleCounter);
                    tDataBufferPointer += tXScaleCounter;
                } else if (tXScale == -1) {
                    // compress by factor 1.5 - every second value is the average of the next two values
                    tDataBufferPointer++;
                    tXScaleCounter--;
                    if (tXScaleCounter < 0) {
                        if (tValue != DISPLAYBUFFER_INVISIBLE_VALUE) {
                            // get average of actual and next value
                            tValue += getDisplayFrowRawInputValue(*tDataBufferPointer++);
                            tValue /= 2;
                        }
                        tXScaleCounter = 1;
                    }
                } else if (tXScale == 1) {
                    tDataBufferPointer++;
                    // expand by factor 1.5 - every second value will be shown 2 times
                    tXScaleCounter--; // starts with 1
                    if (tXScaleCounter < 0) {
                        tDataBufferPointer--;
                        tXScaleCounter = 2;
                    }
                } else {
                    // expand - show value several times
                    if (tXScaleCounter == 0) {
                        tDataBufferPointer++;
                        tXScaleCounter = tXScale;
                    }
                    tXScaleCounter--;
                }
            }

            // draw trigger state line (aka Digital mode)
            if (DisplayControl.showTriggerInfoLine) {
#ifdef LOCAL_DISPLAY_EXISTS
                if (aClearBeforeColor > 0) {
                    LocalDisplay.drawPixel(i, *ScreenBufferWritePointer2, aClearBeforeColor);
                }
#endif
                if (tValue > tTriggerValue) {
#ifdef LOCAL_DISPLAY_EXISTS
                    LocalDisplay.drawPixel(i, tTriggerValue - TRIGGER_HIGH_DISPLAY_OFFSET, COLOR_DATA_TRIGGER);
#endif
                    *ScreenBufferWritePointer2++ = tTriggerValue - TRIGGER_HIGH_DISPLAY_OFFSET;
                }
            }

#ifdef LOCAL_DISPLAY_EXISTS
            if (DisplayControl.drawPixelMode || i == 0) {
                /*
                 * Pixel Mode or first value of chart
                 */
                if (aDrawMode != DRAW_MODE_REGULAR && tCounterForTimingGridRestore == TIMING_GRID_WIDTH) {
                    // Restore grid pixel instead of clearing it
                    tCounterForTimingGridRestore = 0;
                    LocalDisplay.drawPixel(i, tValue, COLOR_GRID_LINES);
                } else {
                    tCounterForTimingGridRestore++;
                    if (aClearBeforeColor > 0) {
                        int tValueClear = *ScreenBufferReadPointer;
                        if (tValueClear != DISPLAYBUFFER_INVISIBLE_VALUE) {
                            LocalDisplay.drawPixel(i, tValueClear, aClearBeforeColor);
                        }

                        if (!DisplayControl.drawPixelMode) {
                            // i == 0 and line mode here. Erase first line in advance and set pointer
                            tLastValueClear = tValueClear;
                            ScreenBufferReadPointer++;
                            tValueClear = *ScreenBufferReadPointer;
                            if (tValueClear != DISPLAYBUFFER_INVISIBLE_VALUE) {
                                // Clear line from 0
                                LocalDisplay.drawLineFastOneX(0, tLastValueClear, tValueClear, aClearBeforeColor);
                            }
                            tLastValueClear = tValueClear; // second value in buffer
                        }
                    }
                    if (tValue != DISPLAYBUFFER_INVISIBLE_VALUE) {
                        LocalDisplay.drawPixel(i, tValue, aColor);
                    }
                }
            } else {
                /*
                 * Line mode here
                 */
                if (aClearBeforeColor > 0 && i != DSO_DISPLAY_WIDTH - 1) {
                    // erase one x value in advance in order not to overwrite the x+1 part of line just drawn before
                    int tValueClear = *ScreenBufferReadPointer;
                    if (tLastValueClear != DISPLAYBUFFER_INVISIBLE_VALUE
                            && tValueClear != DISPLAYBUFFER_INVISIBLE_VALUE) {
                        LocalDisplay.drawLineFastOneX(i, tLastValueClear, tValueClear, aClearBeforeColor);
                    }
                    tLastValueClear = tValueClear;
                }
                // tLastValue is initialized on i == 0 down below
                if (tValue != DISPLAYBUFFER_INVISIBLE_VALUE) {
                    if (tLastValue != DISPLAYBUFFER_INVISIBLE_VALUE) {
                        // Normal mode - draw line
                        if (tLastValue == tValue && (tValue == DISPLAY_VALUE_FOR_ZERO || tValue == 0)) {
                            // clipping occurs draw red line
                            LocalDisplay.drawLineFastOneX(i - 1, tLastValue, tValue, COLOR_DATA_RUN_CLIPPING);
                        } else {
                            LocalDisplay.drawLineFastOneX(i - 1, tLastValue, tValue, aColor);
                        }
                    } else {
                        // first visible value just draw start pixel
                        LocalDisplay.drawPixel(i, tValue, aColor);
                    }
                }
            }
            tLastValue = tValue;
#endif
            // store data in screen buffer
            *ScreenBufferWritePointer1 = tValue;
            ScreenBufferWritePointer1++;
            ScreenBufferReadPointer++;
        }
        if (tProcessMaxValues) {
            /*
             * Print max values. Use chart index 0. Do not draw direct for BlueDisplay if isEffectiveMinMaxMode
             * Loop again for rendering minimums if isEffectiveMinMaxMode
             */
            BlueDisplay1.drawChartByteBuffer(0, 0, aColor, aClearBeforeColor, 0, !aDrawAlsoMin,
                    &DisplayBuffer[0], aLength);
            if (aDrawAlsoMin) {
                // Initialize for second loop (min values)
                ScreenBufferReadPointer = &DisplayBufferMin[0];
                ScreenBufferWritePointer1 = &DisplayBufferMin[0];
                tDataBufferPointer = aDataBufferPointer + DATABUFFER_MIN_OFFSET;
                tProcessMaxValues = false;
            } else {
                break;
            }
        } else {
            // Print min values. Use chart index 1. Render direct.
            BlueDisplay1.drawChartByteBuffer(0, 0, aColor, aClearBeforeColor, 1, true, &DisplayBufferMin[0], aLength);
            break;
        }
    } while (true);
}

/**
 * Draws all chart values till DataBufferNextInPointer is reached - used for drawing while acquiring
 * @param aDrawColor
 */
void drawRemainingDataBufferValues(Color_t aDrawColor) {
    // Check needed because of last acquisition, which uses the whole data buffer
    while (DataBufferControl.DataBufferNextDrawPointer < DataBufferControl.DataBufferNextInPointer
            && DataBufferControl.DataBufferNextDrawPointer <= &DataBufferControl.DataBuffer[DATABUFFER_DISPLAY_END]
            && !MeasurementControl.TriggerPhaseJustEnded && !DataBufferControl.DataBufferPreTriggerAreaWrapAround) {

        int tDisplayX = DataBufferControl.NextDrawXValue;
        if (DataBufferControl.DataBufferNextDrawPointer
                == &DataBufferControl.DataBuffer[DATABUFFER_PRE_TRIGGER_SIZE + FFT_SIZE - 1]) {
            // now data buffer is filled with more than 256 samples -> show fft
            draw128FFTValuesFast(COLOR_FFT_DATA);
        }

        // wrap around in display buffer
        if (tDisplayX >= DSO_DISPLAY_WIDTH) {
            tDisplayX = 0;
        }
        DataBufferControl.NextDrawXValue = tDisplayX + 1;

        // unsigned is faster
        unsigned int tValue = DisplayBuffer[tDisplayX];
        unsigned int tValueMin = DisplayBufferMin[tDisplayX];
        int tLastValue;
        int tNextValue;

        /*
         * clear old pixel / line
         */
#ifdef LOCAL_DISPLAY_EXISTS
        if (DisplayControl.drawPixelMode) {
            // new values in data buffer => draw one pixel
            // clear pixel or restore grid
            Color_t tColor = DisplayControl.EraseColor;
            if (tDisplayX % TIMING_GRID_WIDTH == TIMING_GRID_WIDTH - 1) {
                tColor = COLOR_GRID_LINES;
            }
            if (tValue != DISPLAYBUFFER_INVISIBLE_VALUE) {
                BlueDisplay1.drawPixel(tDisplayX, tValue, tColor);
                if (MeasurementControl.isEffectiveMinMaxMode) {
                    BlueDisplay1.drawPixel(tDisplayX, tValueMin, tColor);
                }
            }
        } else {
#endif
            if (tDisplayX < DSO_DISPLAY_WIDTH - 1) {
                // fetch next value and clear line in advance
                tNextValue = DisplayBuffer[tDisplayX + 1];
                if (tNextValue != DISPLAYBUFFER_INVISIBLE_VALUE) {
                    if (tValue != DISPLAYBUFFER_INVISIBLE_VALUE) {
                        // normal mode
                        BlueDisplay1.drawLineFastOneX(tDisplayX, tValue, tNextValue, DisplayControl.EraseColor);
                        if (MeasurementControl.isEffectiveMinMaxMode) {
                            BlueDisplay1.drawLineFastOneX(tDisplayX, tValueMin, DisplayBufferMin[tDisplayX + 1],
                                    DisplayControl.EraseColor);
                        }
                    } else {
                        // first visible value, clear only start pixel
                        BlueDisplay1.drawPixel(tDisplayX + 1, tNextValue, DisplayControl.EraseColor);
                        if (MeasurementControl.isEffectiveMinMaxMode) {
                            BlueDisplay1.drawPixel(tDisplayX + 1, DisplayBufferMin[tDisplayX + 1],
                                    DisplayControl.EraseColor);
                        }
                    }
                }
            }
#ifdef LOCAL_DISPLAY_EXISTS
        }
#endif

        /*
         * get new value
         */
        tValue = getDisplayFrowRawInputValue(*DataBufferControl.DataBufferNextDrawPointer);
        DisplayBuffer[tDisplayX] = tValue;
        if (MeasurementControl.isEffectiveMinMaxMode) {
            tValueMin = getDisplayFrowRawInputValue(
                    *(DataBufferControl.DataBufferNextDrawPointer + DATABUFFER_MIN_OFFSET));
            DisplayBufferMin[tDisplayX] = tValueMin;
        }

#ifdef LOCAL_DISPLAY_EXISTS
        if (DisplayControl.drawPixelMode) {
            if (tValue != DISPLAYBUFFER_INVISIBLE_VALUE) {
                //draw new pixel
                BlueDisplay1.drawPixel(tDisplayX, tValue, aDrawColor);
                if (MeasurementControl.isEffectiveMinMaxMode) {
                    BlueDisplay1.drawPixel(tDisplayX, tValueMin, aDrawColor);
                }
            }
        } else {
#endif
            if (tDisplayX != 0 && tDisplayX <= DSO_DISPLAY_WIDTH - 1) {
                // get lastValue and draw line
                if (tValue != DISPLAYBUFFER_INVISIBLE_VALUE) {
                    tLastValue = DisplayBuffer[tDisplayX - 1];
                    if (tLastValue != DISPLAYBUFFER_INVISIBLE_VALUE) {
                        // normal mode
                        BlueDisplay1.drawLineFastOneX(tDisplayX - 1, tLastValue, tValue, aDrawColor);
                        if (MeasurementControl.isEffectiveMinMaxMode) {
                            BlueDisplay1.drawLineFastOneX(tDisplayX - 1, DisplayBufferMin[tDisplayX - 1], tValueMin,
                                    aDrawColor);
                        }
                    } else {
                        // first visible value, draw only start pixel
                        BlueDisplay1.drawPixel(tDisplayX, tValue, aDrawColor);
                        if (MeasurementControl.isEffectiveMinMaxMode) {
                            BlueDisplay1.drawPixel(tDisplayX, tValueMin, aDrawColor);
                        }
                    }
                }
            }
#ifdef LOCAL_DISPLAY_EXISTS
        }
#endif
        DataBufferControl.DataBufferNextDrawPointer++;
    }
}

/*
 * Show FFT use display buffer
 */
void drawFFT(void) {
    // compute and draw FFT
    BlueDisplay1.clearDisplay(COLOR_BACKGROUND_DSO);
    float *tFFTDataPointer = computeFFT(DataBufferControl.DataBufferDisplayStart);
    // init and draw chart 12 milliseconds with -O0
    // display with Xscale = 2
    ChartFFT.initChart(4 * TEXT_SIZE_11_WIDTH, DSO_DISPLAY_HEIGHT - 2 * TEXT_SIZE_11_HEIGHT, FFT_SIZE, 32 * 5, 2,
    true, 64, 32);
    ChartFFT.initChartColors(COLOR_FFT_DATA, COLOR_RED, RGB(0xC0, 0xC0, 0xC0), COLOR_RED, COLOR_BACKGROUND_DSO);
    // compute Label for x Frequency axis
    char tFreqUnitString[4] = { " Hz" };
    float tTimebaseExactValue = getDataBufferTimebaseExactValueMicros(MeasurementControl.TimebaseEffectiveIndex);
    // compute frequency for 32.th bin (1/4 of nyquist frequency at 256 samples)
    int tFreqAtBin32 = 4000000 / tTimebaseExactValue;
    // draw x axis
    if (tFreqAtBin32 >= 1000) {
        tFreqAtBin32 /= 1000;
        tFreqUnitString[0] = 'k'; // kHz
    }
    ChartFFT.initXLabelInt(0, tFreqAtBin32 * FFT_DISPLAY_SCALE_FACTOR_X, FFT_DISPLAY_SCALE_FACTOR_X, 4);
    ChartFFT.setXTitleText(tFreqUnitString);
    // display 1.0 for input value of tMaxValue -> normalize while drawing chart
    ChartFFT.initYLabelFloat(0, 0.2, 1.0 / FFTInfo.MaxValue, 3, 1);
    ChartFFT.drawAxesAndGrid();
    // show chart
    ChartFFT.drawChartDataFloat(tFFTDataPointer, tFFTDataPointer + FFT_SIZE, CHART_MODE_AREA);
    ChartFFT.drawXAxisTitle();
    /*
     * Print max bin frequency information
     */
    // compute frequency of max bin
    float tFreqAtMaxBin = FFTInfo.MaxIndex * 125000 / tTimebaseExactValue;
    float tFreqDeltaHalf;
    if (tFreqAtMaxBin >= 10000) {
        tFreqAtMaxBin /= 1000;
        tFreqUnitString[0] = 'k'; // kHz
        tFreqDeltaHalf = 62.500;

    } else {
        tFreqUnitString[0] = ' '; // Hz
        tFreqDeltaHalf = 62500;
    }
    snprintf(StringBuffer, sizeof(StringBuffer), "%0.2f%s", tFreqAtMaxBin, tFreqUnitString);
    BlueDisplay1.drawText(140, 4 * TEXT_SIZE_11_HEIGHT + TEXT_SIZE_22_ASCEND, StringBuffer, TEXT_SIZE_22, COLOR_RED,
    COLOR_BACKGROUND_DSO);
    tFreqDeltaHalf /= tTimebaseExactValue; // = tFreqAtBin32 / 64;
    snprintf(StringBuffer, sizeof(StringBuffer), "[\xB1%0.2f%s]", tFreqDeltaHalf, tFreqUnitString);
    BlueDisplay1.drawText(140, 6 * TEXT_SIZE_11_HEIGHT + TEXT_SIZE_11_ASCEND, StringBuffer, TEXT_SIZE_11, COLOR_RED,
    COLOR_BACKGROUND_DSO);
}

/**
 * Draws area chart for fft values. 3 pixel for each value (for a 320 pixel screen)
 * Data is scaled to max = HORIZONTAL_GRID_HEIGHT pixel high and drawn at bottom of screen
 */
void draw128FFTValuesFast(Color_t aColor) {
    if (DisplayControl.ShowFFT) {
        float *tFFTDataPointer = computeFFT(DataBufferControl.DataBufferDisplayStart);

        uint8_t *tDisplayBufferPtr = &DisplayBufferFFT[0];
        float tInputValue;
        uint16_t tDisplayY, DisplayYOld;

        MeasurementControl.MaxFFTValue = FFTInfo.MaxValue;
        // compute frequency of max bin
        MeasurementControl.FrequencyHertzAtMaxFFTBin = FFTInfo.MaxIndex * 125000
                / getDataBufferTimebaseExactValueMicros(MeasurementControl.TimebaseEffectiveIndex);

        //compute scale factor
        float tYDisplayScaleFactor = HORIZONTAL_GRID_HEIGHT / FFTInfo.MaxValue;
        for (int tDisplayX = 0; tDisplayX < DSO_DISPLAY_WIDTH; tDisplayX += 3) {
            /*
             *  get data and perform scaling
             */
            tInputValue = *tFFTDataPointer++;
            tDisplayY = tYDisplayScaleFactor * tInputValue;
            // tDisplayY now from 0 to 32
            tDisplayY = DSO_DISPLAY_HEIGHT - tDisplayY;
            DisplayYOld = *tDisplayBufferPtr;
            if (tDisplayY < DisplayYOld) {
                // increase old bar
                BlueDisplay1.fillRect(tDisplayX, tDisplayY, tDisplayX + 2, DisplayYOld, aColor);
            } else if (tDisplayY > DisplayYOld) {
                // Remove part of old bar
                BlueDisplay1.fillRect(tDisplayX, DisplayYOld, tDisplayX + 2, tDisplayY, COLOR_BACKGROUND_DSO);
            }
            *tDisplayBufferPtr++ = tDisplayY;
        }
    }
}

void clearFFTValuesOnDisplay(void) {
    // Clear old fft area
    BlueDisplay1.fillRectRel(0, DSO_DISPLAY_HEIGHT - HORIZONTAL_GRID_HEIGHT, DSO_DISPLAY_WIDTH,
    HORIZONTAL_GRID_HEIGHT,
    COLOR_BACKGROUND_DSO);
}

/************************************************************************
 * Text output section
 ************************************************************************/
void clearInfo(void) {
    BlueDisplay1.fillRectRel(0, 0, DSO_DISPLAY_WIDTH, 3 * FONT_SIZE_INFO_LONG + 1, COLOR_BACKGROUND_DSO);
}

/*
 * Output info line
 */
void printInfo(void) {
    if (DisplayControl.DisplayPage != CHART || DisplayControl.showInfoMode == INFO_MODE_NO_INFO) {
        return;
    }

// compute value here, because min and max can have changed by completing another measurement,
// while printing first line to screen
    int tValueDiff = MeasurementControl.RawValueMax - MeasurementControl.RawValueMin;
// compensate for aValue -= RawDSOReadingACZero; in getFloatFromRawValue()
    if (MeasurementControl.ChannelIsACMode) {
        tValueDiff += MeasurementControl.RawDSOReadingACZero;
    }

    /*
     * render period + frequency
     */
    char tBufferForPeriodAndFrequency[20];
    float tMicrosPeriod = MeasurementControl.PeriodMicros;
    char tPeriodUnitChar = 0xB5; // micro
    if (tMicrosPeriod >= 50000) {
        tMicrosPeriod = tMicrosPeriod / 1000;
        tPeriodUnitChar = 'm'; // milli
    }

    // values for period >= 10000
    int tPeriodStringLength = 7;
    int tPeriodStringPrecision = 0;
    int tThousandIndex = 13;
    int tThousandIndexHz = 3;
    int tFreqStringLength = 7;
    // "  9.999Hz  21.345us"

    /*
     * Timebase
     */
    char tTimebaseUnitChar;
    int tUnitsPerGrid;
    if (MeasurementControl.TimebaseEffectiveIndex >= TIMEBASE_INDEX_MILLIS) {
        tTimebaseUnitChar = 'm';
    } else if (MeasurementControl.TimebaseEffectiveIndex >= TIMEBASE_INDEX_MICROS) {
        tTimebaseUnitChar = 0xB5; // micro
    } else {
        tTimebaseUnitChar = 'n'; // nano
    }
    tUnitsPerGrid = TimebaseDivValues[MeasurementControl.TimebaseEffectiveIndex];
// number of digits to be printed after the decimal point
    int tPrecision = 3;
    if ((MeasurementControl.ChannelIsACMode && MeasurementControl.DisplayRangeIndexForPrint >= 8)
            || MeasurementControl.DisplayRangeIndexForPrint >= 10) {
        tPrecision = 2;
    }
    if (MeasurementControl.ChannelIsACMode && MeasurementControl.DisplayRangeIndexForPrint >= 11) {
        tPrecision = 1;
    }

    if (tMicrosPeriod < 100) {
        // >= 10.00Hz " 1000.000Hz  1,00us"
        tPeriodStringPrecision = 2;
        tFreqStringLength = 9; // one trailing space
        tThousandIndexHz = 5;
        tPeriodStringLength = 5;
    } else if (tMicrosPeriod < 10000) {
        // "  9.999Hz 1.345,7us"
        tPeriodStringPrecision = 1;
        tThousandIndex = 11;
    }
    /*
     * 9 or 7 character for frequency with separator
     * 1 space
     * 9 character for period e.g "5.300,0us" or " 33.000us"
     */
    snprintf(tBufferForPeriodAndFrequency, sizeof tBufferForPeriodAndFrequency, "%*luHz %*.*f%cs", tFreqStringLength,
            MeasurementControl.FrequencyHertz, tPeriodStringLength, tPeriodStringPrecision, tMicrosPeriod,
            tPeriodUnitChar);
    if (MeasurementControl.FrequencyHertz >= 1000) {
        // set separator for thousands
        formatThousandSeparator(&tBufferForPeriodAndFrequency[0], &tBufferForPeriodAndFrequency[tThousandIndexHz]);
    }
    if (tMicrosPeriod >= 1000) {
        // set separator for thousands
        formatThousandSeparator(&tBufferForPeriodAndFrequency[9], &tBufferForPeriodAndFrequency[tThousandIndex]);
    }

    if (DisplayControl.showInfoMode == INFO_MODE_LONG_INFO) {
        /**
         * Long info mode
         */
        if (MeasurementControl.isSingleShotMode) {
            // current value
            snprintf(StringBuffer, sizeof StringBuffer, "Current=%4.3fV waiting for %s",
                    getFloatFromRawValue(MeasurementControl.RawValueBeforeTrigger),
                    TriggerStatusStrings[MeasurementControl.TriggerStatus]);
        } else {

            // First line
            // Min + Average + Max + Peak-to-peak
            snprintf(StringBuffer, sizeof StringBuffer, "Av%6.*fV Min%6.*f Max%6.*f P2P%6.*fV", tPrecision,
                    getFloatFromRawValue(MeasurementControl.RawValueAverage), tPrecision,
                    getFloatFromRawValue(MeasurementControl.RawValueMin), tPrecision,
                    getFloatFromRawValue(MeasurementControl.RawValueMax), tPrecision,
                    getFloatFromRawValue(tValueDiff));
        }
        BlueDisplay1.drawText(0, FONT_SIZE_INFO_LONG_ASC, StringBuffer, FONT_SIZE_INFO_LONG, COLOR_BLACK,
        COLOR_INFO_BACKGROUND);

        // Second line
        // XScale + Timebase + MicrosPerPeriod + Hertz + Channel
        const char * tChannelString = ADCInputMUXChannelStrings[MeasurementControl.ADCInputMUXChannelIndex];
#ifdef LOCAL_DISPLAY_EXISTS
        if (MeasurementControl.ADS7846ChannelsAsDatasource) {
            tChannelString = ADS7846ChannelStrings[MeasurementControl.ADCInputMUXChannelIndex];
        }
#endif
        getScaleFactorAsString(&StringBuffer[40], DisplayControl.XScale);
        snprintf(StringBuffer, sizeof StringBuffer, "%s %4u%cs %s %s", &StringBuffer[40], tUnitsPerGrid,
                tTimebaseUnitChar, tBufferForPeriodAndFrequency, tChannelString);
        BlueDisplay1.drawText(0, FONT_SIZE_INFO_LONG_ASC + FONT_SIZE_INFO_LONG, StringBuffer, FONT_SIZE_INFO_LONG,
        COLOR_BLACK, COLOR_INFO_BACKGROUND);

        // Third line
        // Trigger: Slope + Mode + Level + FFT max frequency - Empty space after string is needed for voltage picker value
        if (DisplayControl.ShowFFT) {
            snprintf(tBufferForPeriodAndFrequency, sizeof tBufferForPeriodAndFrequency, " %6.0fHz %4.1f",
                    MeasurementControl.FrequencyHertzAtMaxFFTBin, MeasurementControl.MaxFFTValue);
            if (MeasurementControl.FrequencyHertzAtMaxFFTBin >= 1000) {
                formatThousandSeparator(&tBufferForPeriodAndFrequency[0], &tBufferForPeriodAndFrequency[3]);
            }
        } else {
            memset(tBufferForPeriodAndFrequency, ' ', 9);
            tBufferForPeriodAndFrequency[9] = '\0';
        }

        char tSlopeChar;
        char tTriggerAutoChar;
        if (MeasurementControl.TriggerSlopeRising) {
            tSlopeChar = 0xD1; //ascending
        } else {
            tSlopeChar = 0xD2; //Descending
        }

        switch (MeasurementControl.TriggerMode) {
        case TRIGGER_MODE_AUTOMATIC:
            tTriggerAutoChar = 'A';
            break;
        case TRIGGER_MODE_MANUAL:
            tTriggerAutoChar = 'M';
            break;
        default:
            tTriggerAutoChar = 'O';
            break;
        }

        snprintf(StringBuffer, sizeof StringBuffer, "Trigg: %c %c %5.*fV %s", tSlopeChar, tTriggerAutoChar,
                tPrecision - 1, getFloatFromRawValue(MeasurementControl.RawTriggerLevel),
                tBufferForPeriodAndFrequency);
        BlueDisplay1.drawText(0, FONT_SIZE_INFO_LONG_ASC + (2 * FONT_SIZE_INFO_LONG), StringBuffer,
        FONT_SIZE_INFO_LONG, COLOR_BLACK, COLOR_INFO_BACKGROUND);

        // Debug infos
//		char tTriggerTimeoutChar = 0x20; // space
//		if (MeasurementControl.TriggerStatus < 2) {
//			tTriggerTimeoutChar = 0x21;
//		}
//		snprintf(StringBuffer, sizeof StringBuffer, "%c TO=%6d DB=%x S=%x E=%x", tTriggerTimeoutChar,
//				MeasurementControl.TriggerTimeoutSampleOrLoopCount,
//				(uint16_t) ((uint32_t) &DataBufferControl.DataBuffer[0]) & 0xFFFF,
//				(uint16_t) ((uint32_t) DataBufferControl.DataBufferDisplayStart) & 0xFFFF,
//				(uint16_t) ((uint32_t) DataBufferControl.DataBufferEndPointer) & 0xFFFF);
        // Print RAW min max average
//		snprintf(StringBuffer, sizeof StringBuffer, "Min%#X Average%#X Max%#X", MeasurementControl.ValueMin,
//				MeasurementControl.ValueAverage, MeasurementControl.ValueMax);
//
//		BlueDisplay1.drawText(INFO_LEFT_MARGIN, INFO_UPPER_MARGIN + (3 * TEXT_SIZE_11_HEIGHT), StringBuffer, 1, COLOR_BLUE, COLOR_INFO_BACKGROUND);
        // Debug end

    } else {
        /*
         * Short version
         */
#ifdef LOCAL_DISPLAY_EXISTS
        snprintf(StringBuffer, sizeof StringBuffer, "%6.*fV %6.*fV%s%4u%cs", tPrecision,
                getFloatFromRawValue(MeasurementControl.RawValueAverage), tPrecision,
                getFloatFromRawValue(tValueDiff), tBufferForPeriodAndFrequency, tUnitsPerGrid, tTimebaseUnitChar);
#else
        snprintf(StringBuffer, sizeof StringBuffer, "%6.*fV %6.*fV  %6luHz %4u%cs", tPrecision,
                getFloatFromRawValue(MeasurementControl.RawValueAverage), tPrecision,
                getFloatFromRawValue(tValueDiff), MeasurementControl.FrequencyHertz, tUnitsPerGrid,
                tTimebaseUnitChar);
        if (MeasurementControl.FrequencyHertz >= 1000) {
            // set separator for thousands
            formatThousandSeparator(&StringBuffer[16], &StringBuffer[19]);
        }
#endif

        BlueDisplay1.drawText(0, FONT_SIZE_INFO_SHORT_ASC, StringBuffer, FONT_SIZE_INFO_SHORT, COLOR_BLACK,
        COLOR_INFO_BACKGROUND);
    }
}

/**
 * prints only trigger value
 */
void printTriggerInfo(void) {
    int tPrecision = 2;
    if ((MeasurementControl.ChannelIsACMode && MeasurementControl.DisplayRangeIndex >= 8)
            || MeasurementControl.DisplayRangeIndex >= 10) {
        tPrecision = 1;
    }
    if (MeasurementControl.ChannelIsACMode && MeasurementControl.DisplayRangeIndex >= 11) {
        tPrecision = 0;
    }
    snprintf(StringBuffer, sizeof StringBuffer, "%5.*fV", tPrecision,
            getFloatFromRawValue(MeasurementControl.RawTriggerLevel));

    int tYPos = TRIGGER_LEVEL_INFO_SHORT_Y;
    int tXPos = TRIGGER_LEVEL_INFO_SHORT_X;
    int tFontsize = FONT_SIZE_INFO_SHORT;
    if (DisplayControl.showInfoMode == INFO_MODE_LONG_INFO) {
        tXPos = TRIGGER_LEVEL_INFO_LONG_X;
        tYPos = TRIGGER_LEVEL_INFO_LONG_Y;
        tFontsize = FONT_SIZE_INFO_LONG;
    }

    BlueDisplay1.drawText(tXPos, tYPos, StringBuffer, tFontsize, COLOR_BLACK, COLOR_INFO_BACKGROUND);
}

/*******************************
 * RAW to display value section
 *******************************/

/**
 * @param aAdcValue raw ADC value
 * @return Display value (0 to 240-DISPLAY_VALUE_FOR_ZERO) or 0 if raw value to high
 */
int getDisplayFrowRawInputValue(int aAdcValue) {
    if (aAdcValue == DATABUFFER_INVISIBLE_RAW_VALUE) {
        return DISPLAYBUFFER_INVISIBLE_VALUE;
    }
// 1. convert raw to signed values if ac range is selected
    if (MeasurementControl.ChannelIsACMode) {
        aAdcValue -= MeasurementControl.RawDSOReadingACZero;
    }

// 2. adjust with display range offset
    aAdcValue = aAdcValue - MeasurementControl.RawOffsetValueForDisplayRange;
    if (aAdcValue < 0) {
        return DISPLAY_VALUE_FOR_ZERO;
    }

// 3. convert raw to display value
    aAdcValue *= ScaleFactorRawToDisplayShift18[MeasurementControl.DisplayRangeIndex];
    aAdcValue >>= DSO_SCALE_FACTOR_SHIFT;

// 4. invert and clip value
    if (aAdcValue > DISPLAY_VALUE_FOR_ZERO) {
        aAdcValue = 0;
    } else {
        aAdcValue = (DISPLAY_VALUE_FOR_ZERO) - aAdcValue;
    }
    return aAdcValue;
}

/**
 *
 * @param aAdcValuePtr Data pointer
 * @param aCount number of samples for oversampling
 * @return average of count values from data pointer
 */
int getDisplayFrowMultipleRawValues(uint16_t * aAdcValuePtr, int aCount) {
//
    int tAdcValue = 0;
    for (int i = 0; i < aCount; ++i) {
        tAdcValue += *aAdcValuePtr++;
    }
    return getDisplayFrowRawInputValue(tAdcValue / aCount);
}

int getRawOffsetValueFromGridCount(int aCount) {
    aCount = (aCount * HORIZONTAL_GRID_HEIGHT) << DSO_SCALE_FACTOR_SHIFT;
    aCount = aCount / ScaleFactorRawToDisplayShift18[MeasurementControl.DisplayRangeIndex];
    return aCount;
}

/*
 * get raw value for display value - assume InputRangeIndex
 * aValue raw display value 0 is top
 */
int getInputRawFromDisplayValue(int aValue) {
// invert value
    aValue = (DISPLAY_VALUE_FOR_ZERO) - aValue;

// convert to raw
    aValue = aValue << DSO_SCALE_FACTOR_SHIFT;
    aValue /= ScaleFactorRawToDisplayShift18[MeasurementControl.DisplayRangeIndex];

//adjust with offset
    aValue += MeasurementControl.RawOffsetValueForDisplayRange;

// adjust for zero offset
    if (MeasurementControl.ChannelIsACMode) {
        aValue += MeasurementControl.RawDSOReadingACZero;
    }
    return aValue;
}

/*
 * computes corresponding voltage from raw value
 */
float getFloatFromRawValue(int aValue) {
    if (MeasurementControl.ChannelIsACMode) {
        aValue -= MeasurementControl.RawDSOReadingACZero;
    }
    return (MeasurementControl.actualDSORawToVoltFactor * aValue);
}

/*
 * computes corresponding voltage from display y position
 */
float getFloatFromDisplayValue(uint8_t aDisplayValue) {
    int tRawValue = getInputRawFromDisplayValue(aDisplayValue);
    return getFloatFromRawValue(tRawValue);
}

/*
 * for internal test of the conversion routines
 */
void testDSOConversions(void) {
// Prerequisites
    MeasurementControl.ChannelIsACMode = false;
    MeasurementControl.DisplayRangeIndex = 3;    // 0,1 Volt / div | Raw-136 / div | 827 max
    MeasurementControl.RawOffsetValueForDisplayRange = 100;
    autoACZeroCalibration();
    initRawToDisplayFactorsAndMaxPeakToPeakValues();

// Tests of raw <-> display conversion routines
    int tValue;
    tValue = getRawOffsetValueFromGridCount(3);    // appr. 416

    tValue = getDisplayFrowRawInputValue(400);
    tValue = getInputRawFromDisplayValue(tValue);

    MeasurementControl.ChannelIsACMode = true;

    tValue = getDisplayFrowRawInputValue(2200);    // since it is AC range
    tValue = getInputRawFromDisplayValue(tValue);

    float tFloatValue;
    tFloatValue = getFloatFromRawValue(400 + MeasurementControl.RawDSOReadingACZero);
    tFloatValue = getFloatFromDisplayValue(getDisplayFrowRawInputValue(400));
    tFloatValue = sADCToVoltFactor * 400;
    tFloatValue = tFloatValue / sADCToVoltFactor;
// to see above result in debugger
    tFloatValue = tFloatValue * 2;
}