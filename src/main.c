#include <pebble.h>
enum {
	BUILD_NUMBER = 10,
	WINDOWnum_START = 0,
	WINDOWnum_PLANET = 1,
	WINDOWnum_MENU = 2,
	WINDOWnum_BRIGHTNESS = 3,
	GPS_AWAITING_DATA = 0,
	DISPLAY_COORDS = 1,
	DISPLAY_COMPASS = 2,
	DISPLAY_INCLINATION = 3
};		// Declaration of window numbers for click handling services, and header states for compass/inclination readings, and the build number
enum {
	KEY_LATITUDE = 1,
	KEY_LONGITUDE = 2,
	KEY_USEOLDDATA = 3,
	KEY_UTCh = 4
};		// Javascript dictiopnary keys for GPS get
enum {
	LAT_STORED = 0,
	LNG_STORED = 1,
	UTC_STORED = 2,
	LAST_DATE_STORED = 3,
	LAST_TIME_STORED = 4,
	VERSION_NUMBER = 5
};		// Persistent storage keys

// UI declarations -------------------------------------------------------------------------------------------------------------------------------------------------------
static Window *s_start_window;			// Declare the starting screen - Displays old gps data if existent, has options of updating, or going straight to brightness calcs
static Window *s_planet_window;			// Declare the planet screen, the main screen
static Window *s_menu_window;				// Declare the menu screen
static Window *s_brightness_window;	// Declare the brightness screen
static TextLayer *s_lastCoord_layer;		// Declare the text layer that houses the last gps update/coords
static TextLayer *s_belowCoord_layer;		// Declare a text layer that sits below the last gps update banner
static TextLayer *s_genericText1_layer;		// Declare a generic text layer that can be used in various windows
static TextLayer *s_genericText2_layer;		// Declare another generic text layer that can be used in various windows
static TextLayer *s_genericText3_layer;		// Declare another generic text layer that can be used in various windows
static TextLayer *s_genericText4_layer;		// Declare another generic text layer that can be used in various windows
static TextLayer *s_genericText5_layer;		// Declare another generic text layer that can be used in various windows
static TextLayer *s_genericText6_layer;		// Declare another generic text layer that can be used in various windows
static BitmapLayer *s_planetScreenButtons_layer;		// Declare the bitmap layer for the buttons on the planet screen
static GBitmap *s_buttons_bitmap;		// Declare the bitmap image that will be shown on the planet screen
static BitmapLayer *s_compassType_layer;
static GBitmap *s_magNorth_icon;
static GBitmap *s_trueNorth_icon;
static GBitmap *s_emptyNorth_icon;

// Global variable declarations ------------------------------------------------------------------------------------------------------------------------------------------
static uint8_t planetNumber;		// Declare the global variable signifying the planet number for calculation specifics, currently covering 0(Sun) - 7(Neptune)
static float localLat_deg;			// Declare the global variable of the last known latitude
static float localLng_deg;			// Declare the global variable of the last known longitude
static uint32_t cCodeDate;			// Declare the date in code friendly format (YYYYMMDD)
static uint16_t localTime;			// Local time in minutes
static int8_t UToffset;				// The number of hours offset from UTC
static float dDays;					// Declare the modified julian day num
static float ecl;						// Declare the obliquity of the ecliptic for Earth, dependent on d
static float PIconst;					// Declare the constant PI value
static float r_sun;					// Declare the global variable for the distance to the sun in AU
static float xs;							// Declare the global variables for the ecliptic rectangular geocentric coordinates
static float ys;
static float LST;						// Declare the local side real time global variable
static float GMST0;					// Declare the GMST0 global variable for rise/set times
static float M_jupiter;				// Declare the global variables of jupiter and saturn mean anomalies
static float M_saturn;
static const char *planetNamesArr[8];		// Declare the array of planet names, initilised in init
static uint8_t currentWindow;		// Declare a global identifier for which window is currently being displayed, helps the click handling
static uint8_t currentHeader;		// Declare the header number that is being displayed on the planet screen
static uint16_t lastUpdateTime;			// Declare a global variable for the last GPS update time
static uint32_t lastUpdateDate;			// Declare a global variable for the last GPS update date

// Display buffers - use generic buffers that are resized on screen changes to manage memory usage ----------------------------------------------------------------------
static char *aboutBuffer;
static char globalBuffer[7][39];			// The maximum is reached with the brightness screen
static void setAboutBuffer() {
	aboutBuffer = (char*)malloc(203);
	strcpy(aboutBuffer, "Welcome to v1.1\nClick to continue -->\n\nAltitude is angle above the horizon, 0 deg is flush with watchface. Azimuth is bearing from North. Brightness is an inverse log scale, V is visible with naked eye.");
}

 // Small function to display values for debugging purposes -------------------------------------------------------------------------------------------------------------
static void dispVal (int val, char valName[]) {
  //Displays the value
  APP_LOG(APP_LOG_LEVEL_DEBUG, "%s %d", valName, val);
}

// UI setups -------------------------------------------------------------------------------------------------------------------------------------------------------------
static void typicalTextLayer(TextLayer *textlayer, int8_t alignID){
	//APP_LOG(APP_LOG_LEVEL_INFO, "Entered the typicalTextLayer()");
	text_layer_set_text_color(textlayer, GColorWhite);
	text_layer_set_background_color(textlayer, GColorClear);
	text_layer_set_overflow_mode(textlayer, GTextOverflowModeWordWrap);
	text_layer_set_font(textlayer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
	switch (alignID) {
		case 1:
			text_layer_set_text_alignment(textlayer, GTextAlignmentLeft);
			break;
		case 2:
			text_layer_set_text_alignment(textlayer, GTextAlignmentCenter);
			break;
		case 3:
			text_layer_set_text_alignment(textlayer, GTextAlignmentRight);
			break;
		default:
			APP_LOG(APP_LOG_LEVEL_ERROR, "Not the correct alignment passed variable");
	}
}
static void switchHighlightLayer(TextLayer *beforeLayer, TextLayer *selectionLayer, TextLayer *afterLayer) {
	text_layer_set_background_color(beforeLayer, GColorBlack);
	text_layer_set_text_color(beforeLayer, GColorWhite);
	text_layer_set_background_color(selectionLayer, GColorWhite);		// Selection layer reversed
	text_layer_set_text_color(selectionLayer, GColorBlack);
	text_layer_set_background_color(afterLayer, GColorBlack);
	text_layer_set_text_color(afterLayer, GColorWhite);
}
static void start_window_appear (Window *window) {
	// Setup the window's layout here
	currentWindow = WINDOWnum_START;

	// Create the layer that will sit below the coord banner --> Create3
	s_belowCoord_layer = text_layer_create(GRect(0, 45, 144, 123));
	typicalTextLayer(s_belowCoord_layer, 3);
	text_layer_set_font(s_belowCoord_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28));
	// Fill the text now
	strcpy(globalBuffer[0], "Update GPS -->\n\nUse old data -->");
	text_layer_set_text(s_belowCoord_layer, globalBuffer[0]);

	// Add the child layers
	layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_lastCoord_layer));		// Add the last coords text layer to the start window
	layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_belowCoord_layer));
}
static void start_window_disappear (Window *window) {
	//APP_LOG(APP_LOG_LEVEL_INFO, "Entered start window disappear");
	//dispVal((int)heap_bytes_used(), "Bytes used:");
	// Destroy any created text layers in the sister load
	text_layer_destroy(s_belowCoord_layer);
}
static void planet_window_appear (Window *window) {
	//APP_LOG(APP_LOG_LEVEL_INFO, "Entered planet window appear");
	//dispVal((int)heap_bytes_used(), "Bytes used:");
	// Construct the planet window text layers -- the top layer is already created and filled
	currentWindow = WINDOWnum_PLANET;

	// Create the layer that will sit below the coord banner and display the planet name --> Create4
	s_belowCoord_layer = text_layer_create(GRect(0, 26, 133, 30));
	typicalTextLayer(s_belowCoord_layer, 2);
	text_layer_set_font(s_belowCoord_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));

	// Create the overlaid LHS labels text layer --> Create5
	s_genericText1_layer = text_layer_create(GRect(0, 52, 67, 108));
	typicalTextLayer(s_genericText1_layer, 2);
	text_layer_set_font(s_genericText1_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));

	// Create the values LHS labels text layer --> Create6
	s_genericText2_layer = text_layer_create(GRect(0, 62, 67, 98));
	typicalTextLayer(s_genericText2_layer, 2);
	text_layer_set_font(s_genericText2_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));

	// Create the brightness footer text layer --> Create7
	s_genericText3_layer = text_layer_create(GRect(0, 125, 133, 24));
	typicalTextLayer(s_genericText3_layer, 2);

	// Create the overlaid RHS labels text layer --> Create9
	s_genericText4_layer = text_layer_create(GRect(67, 52, 66, 108));
	typicalTextLayer(s_genericText4_layer, 2);
	text_layer_set_font(s_genericText4_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));

	// Create the values RHS labels text layer --> Create10
	s_genericText5_layer = text_layer_create(GRect(67, 62, 66, 98));
	typicalTextLayer(s_genericText5_layer, 2);
	text_layer_set_font(s_genericText5_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));

	// Create the button bitmap --> Create21
	s_buttons_bitmap = gbitmap_create_with_resource(RESOURCE_ID_BUTTON_OPTIONS_ICON);
	s_planetScreenButtons_layer = bitmap_layer_create(GRect(133,0,11,148));
	//bitmap_layer_set_compositing_mode(s_planetScreenButtons_layer, GCompOpAssign);
	bitmap_layer_set_bitmap(s_planetScreenButtons_layer, s_buttons_bitmap);

	// Create the true/magnetic north label text layer
	s_trueNorth_icon = gbitmap_create_with_resource(RESOURCE_ID_TRUE_NORTH_ICON);
	s_magNorth_icon = gbitmap_create_with_resource(RESOURCE_ID_MAG_NORTH_ICON);
	s_emptyNorth_icon = gbitmap_create_with_resource(RESOURCE_ID_EMPTY_BITMAP);
	s_compassType_layer = bitmap_layer_create(GRect(1, 0, 40, 8));
	bitmap_layer_set_background_color(s_compassType_layer, GColorClear);

	// Add child layers
	layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_lastCoord_layer));		// Add the last coords text layer to the planet window
	layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_belowCoord_layer));
	layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_genericText1_layer));
	layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_genericText2_layer));
	layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_genericText3_layer));
	layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_genericText4_layer));
	layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_genericText5_layer));
	layer_add_child(window_get_root_layer(window), bitmap_layer_get_layer(s_planetScreenButtons_layer));
	layer_add_child(window_get_root_layer(window), bitmap_layer_get_layer(s_compassType_layer));
}
static void planet_window_disappear (Window *window) {
	//APP_LOG(APP_LOG_LEVEL_INFO, "Entered planet window disappear");
	//dispVal((int)heap_bytes_used(), "Bytes used:");
	// Unsubscribe from services
	compass_service_unsubscribe();
	accel_data_service_unsubscribe();
	// Destroy the planet window text layers
	gbitmap_destroy(s_trueNorth_icon);
	gbitmap_destroy(s_magNorth_icon);
	gbitmap_destroy(s_emptyNorth_icon);
		bitmap_layer_destroy(s_compassType_layer);
	gbitmap_destroy(s_buttons_bitmap);					// Destroy the Create21
		bitmap_layer_destroy(s_planetScreenButtons_layer);
	text_layer_destroy(s_genericText5_layer);		// Destroy the Create10
	text_layer_destroy(s_genericText4_layer);		// Destroy the Create9
	text_layer_destroy(s_genericText3_layer);		// Destroy the Create7
	text_layer_destroy(s_genericText2_layer);		// Destroy the Create6
	text_layer_destroy(s_genericText1_layer);		// Destroy Create5
	text_layer_destroy(s_belowCoord_layer);		// Destroy Create4

	// Turn off the light
	light_enable(false);
}
static void menu_window_appear (Window *window) {
	// Setup the window's layout here
	currentWindow = WINDOWnum_MENU;

	// Create the layer that will sit below the coord banner --> Create11
	s_belowCoord_layer = text_layer_create(PBL_IF_RECT_ELSE(GRect(0, 0, 144, 168), GRect(35, 16, 125, 165)));     // DIFFERENT COORDS FOR ROUND FACE --> checked
	typicalTextLayer(s_belowCoord_layer, 3);
	text_layer_set_font(s_belowCoord_layer, fonts_get_system_font(PBL_IF_RECT_ELSE(FONT_KEY_GOTHIC_28, FONT_KEY_GOTHIC_24)));
	// Fill the text now
	strcpy(globalBuffer[0], "Brightness ->\n\nUpdate GPS ->\n\nAbout ->");
	text_layer_set_text(s_belowCoord_layer, globalBuffer[0]);

	// Add the child layers
	layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_belowCoord_layer));
}
static void menu_window_disappear (Window *window) {
	// Destroy any created text layers in the sister load
	text_layer_destroy(s_belowCoord_layer);		// Create11
	aboutBuffer = (char*)realloc(aboutBuffer, 1);		// Diminish the about buffer size
}
static void brightness_window_appear (Window *window) {
	//APP_LOG(APP_LOG_LEVEL_INFO, "Entered brightness window appear");
	//dispVal((int)heap_bytes_used(), "Bytes used:");
	// Setup the window's layout here
	currentWindow = WINDOWnum_BRIGHTNESS;
	int textLayerHeights = 22;

	// Create the layer for mercury --> Create13
	s_belowCoord_layer = text_layer_create(GRect(0, 0, 144, textLayerHeights));
	typicalTextLayer(s_belowCoord_layer, 1);
	text_layer_set_text(s_belowCoord_layer, "Calculating");
	// Create the layer for venus --> Create14
	s_genericText1_layer = text_layer_create(GRect(0, textLayerHeights * 1, 144, textLayerHeights));
	typicalTextLayer(s_genericText1_layer, 1);
	// Create the layer for mars --> Create15
	s_genericText2_layer = text_layer_create(GRect(0, textLayerHeights * 2, 144, textLayerHeights));
	typicalTextLayer(s_genericText2_layer, 1);
	// Create the layer for jupiter --> Create16
	s_genericText3_layer = text_layer_create(GRect(0, textLayerHeights * 3, 144, textLayerHeights));
	typicalTextLayer(s_genericText3_layer, 1);
	// Create the layer for saturn --> Create17
	s_genericText4_layer = text_layer_create(GRect(0, textLayerHeights * 4, 144, textLayerHeights));
	typicalTextLayer(s_genericText4_layer, 1);
	// Create the layer for uranus --> Create18
	s_genericText5_layer = text_layer_create(GRect(0, textLayerHeights * 5, 144, textLayerHeights));
	typicalTextLayer(s_genericText5_layer, 1);
	// Create the layer for neptune --> Create19
	s_genericText6_layer = text_layer_create(GRect(0, textLayerHeights * 6, 144, textLayerHeights));
	typicalTextLayer(s_genericText6_layer, 1);

	// Add the child layers
	layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_belowCoord_layer));
	layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_genericText1_layer));
	layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_genericText2_layer));
	layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_genericText3_layer));
	layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_genericText4_layer));
	layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_genericText5_layer));
	layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_genericText6_layer));
}
static void brightness_window_disappear (Window *window) {
	//APP_LOG(APP_LOG_LEVEL_INFO, "Entered brightness window disappear");
	//dispVal((int)heap_bytes_used(), "Bytes used:");
	// Destroy any created text layers in the sister load
	text_layer_destroy(s_genericText6_layer);		// Create19
	text_layer_destroy(s_genericText5_layer);		// Create18
	text_layer_destroy(s_genericText4_layer);		// Create17
	text_layer_destroy(s_genericText3_layer);		// Create16
	text_layer_destroy(s_genericText2_layer);		// Create15
	text_layer_destroy(s_genericText1_layer);		// Create14
	text_layer_destroy(s_belowCoord_layer);		// Create13
}

// Functions pertaining to astronomical calculations ---------------------------------------------------------------------------------------------------------------------
static float calc_d_JDate (uint32_t dateInCodeFormat, uint16_t timeInMinutes, int8_t UTCalteration) {
  //Converts the date given in code format into a julian date set by the reference website
  int16_t yearVar = dateInCodeFormat / 10000;
  int16_t monthVar = (dateInCodeFormat % 10000) / 100;
  int16_t dayVar = dateInCodeFormat % 100;
  float temp = 367 * yearVar - 7 * (yearVar + (monthVar + 9) / 12) / 4 + 275 * monthVar / 9 + dayVar - 730530;

	temp = temp + (float)timeInMinutes/(60*24) - (float)UTCalteration/24;
  return temp;
}		// Transforms a current date and time into the julian date modified
static float modDecimal (float x, float divisor) {
  //A modulus operator that results in positive decimal remainders
  float temp = x;
  if (divisor == 0) {
    //Can't divide by 0!
    temp = 0;    //This should be enough to crush multiplications and flag a division by zero
  } else {
    if (divisor <= 0) {
      divisor = -divisor;
    }
    //Deal with negative numbers first, positive x values will be exempt from this
    while (temp < 0) {
      temp = temp + divisor;    //Add divisor until temp is above 0, this is the remainder
    }
    //Now deal with positive numbers above divisor, negative numbers are already within range and will skip this step
    while (temp > divisor) {
      temp = temp - divisor;    //Subtract divisor until temp is within divisor range
    }
  }

  return temp;
}			// Returns a positive decimal modulus remainder
static float Abs (float x) {
  if (x < 0) {
    x = -x;
  }
  return x;
}
static float Sin (float xDeg) {
  //Unfortunately sin will not work on doubles so this work around will return the sin as a decimal in degrees!
  int32_t trigAngle = TRIG_MAX_ANGLE * (xDeg / 360);
  return (float)sin_lookup(trigAngle) / TRIG_MAX_RATIO;
}
static float Cos (float xDeg) {
  //Same as sin, need to function using the lookup value
  int32_t trigAngle = TRIG_MAX_ANGLE * (xDeg / 360);
  return (float)cos_lookup(trigAngle) / TRIG_MAX_RATIO;
}
static float arctan2 (float ynum, float xnum) {
  //As with the sin and cos functions, arc tan must be altered to keep some form of accuracy along the way
  float xMaxMultiplier = Abs(32767 / xnum);
  float yMaxMultiplier = Abs(32767 / ynum);
  float scaleFactor = 1;      //Initially set scale factor to 1
  if (xMaxMultiplier > yMaxMultiplier) {
    scaleFactor = yMaxMultiplier;    //Can only scale by lowest multiplier
  } else {
    scaleFactor = xMaxMultiplier;
  }
  int16_t yScaled = ynum * scaleFactor;
  int16_t xScaled = xnum * scaleFactor;
  float temp = (float)atan2_lookup(yScaled, xScaled);
  temp = temp / TRIG_MAX_ANGLE * 360;
  return temp;
}
static float powerOfTen (int num) {
	float rst = 1.0;
	if (num >= 0) {
		for(int i = 0; i < num ; i++){
			rst *= 10.0;
		}
	} else {
		for(int i = 0; i < (0 - num ); i++){
			rst *= 0.1;
		}
	}
	return rst;
}
static float my_sqrt (float a) {
	// Thanks to http://www.codeproject.com/Articles/570700/SquareplusRootplusalgorithmplusforplusC for a more accurate sqrt algorithm
	// find more detail of this method on wiki methods_of_computing_square_roots
	// Babylonian method cannot get exact zero but approximately value of the square_root
	float z = a;
	float rst = 0.0;
	int max = 8;     // to define maximum digit
	int i;
	float j = 1.0;
	for(i = max ; i > 0 ; i--){
		// value must be bigger then 0
		if (z - (( 2 * rst ) + ( j * powerOfTen(i)))*( j * powerOfTen(i)) >= 0) {
			while ( z - (( 2 * rst ) + ( j * powerOfTen(i)))*( j * powerOfTen(i)) >= 0) {
				j++;
				if (j >= 10) {break;}
			}
			j--; //correct the extra value by minus one to j
			z -= (( 2 * rst ) + ( j * powerOfTen(i)))*( j * powerOfTen(i)); //find value of z
			rst += j * powerOfTen(i);     // find sum of a
			j = 1.0;
		}
	}

	for (i = 0 ; i >= 0 - max ; i--) {
		if (z - (( 2 * rst ) + ( j * powerOfTen(i)))*( j * powerOfTen(i)) >= 0) {
			while ( z - (( 2 * rst ) + ( j * powerOfTen(i)))*( j * powerOfTen(i)) >= 0) {
				j++;
			}
			j--;
			z -= (( 2 * rst ) + ( j * powerOfTen(i)))*( j * powerOfTen(i)); //find value of z
			rst += j * powerOfTen(i);     // find sum of a
			j = 1.0;
		}
	}
	// find the number on each digit
	return rst;
}
static float logBase10 (float num) {
	// Calculates the logarithm of a number to base 10
	// Thanks to http://fiziko.bureau42.com/teaching_tidbits/manual_logarithms.pdf for the algorithm
	const uint8_t MAX_ITERATIONS = 20;

	float answer = num;
	int seed = 0;
	if (num > 10) {
		while (answer > 10) {
			answer = num / powerOfTen(seed++);			// The seed value represents a log(10^seed), so can be added straight onto the answer
		}
		seed--;		// Seed has to be back adjusted as the last iteration is the one which fails the while loop
	} else if (num < 1) {
		while (answer < 1) {
			answer = num / powerOfTen(seed--);			// As the number is low, the seed decreases
		}
		seed++;		// Seed has to be back adjusted as the last iteration is the one which fails the while loop
	} else {
		seed = 0;			// num is between 1 and 10; ie answer between 0 and 1
	}

	// Now we are left with seed + log(answer) where answer should be between 1 and 10, and log(answer) will be between 0 and 1
	// We will work between a lower bound and upper bound which house answer, and log(lBound) < log(answer) < log(uBound) will converge
	float lBound = 1;
	float uBound = 10;
	float lAnswer = 0;
	float uAnswer = 1;
	float testBound;
	uint8_t steps = 0;
	while (steps++ < MAX_ITERATIONS) {
		testBound = my_sqrt(lBound * uBound);			// First determine the sqrt of the log bounds
		if (testBound < answer) {
			// Test bound is the new lower bound
			lAnswer = (lAnswer + uAnswer) / 2;		// Transform the answer bound
			lBound = testBound;										// Set the new lower bound
		} else {
			// Test bound is the new upper bound
			uAnswer = (lAnswer + uAnswer) / 2;		// Transform the answer bound
			uBound = testBound;										// Set the new upper bound
		}
	}

	return (seed + (lAnswer + uAnswer) / 2);		// Last average to return, add the seed value onto it
}
static float arccos (float xnum) {
  return arctan2(my_sqrt((1 + xnum) * (1 - xnum)), xnum);
}
static float returnOrbitalElement (uint8_t pNum, uint8_t elementNum) {
	// Lookup method to return the orbital method; N = 0, i = 1, w = 2, a = 3, e = 4, M = 5 | pNum is the planet number
	float c = 0;
	float m = 0;
	switch (pNum) {
		case 0:
			switch (elementNum) {
				case 0:			//N
					c = 0;		// Sun N = 0
					m = 0;
					break;
				case 1:			//i
					c = 0;		// Sun i = 0
					m = 0;
					break;
				case 2:			//w
					c = 282.9404;
					m = 0.0000470935;
					break;
				case 3:			//a
					c = 1;		// Sun a = 1 (1 AU)
					m = 0;
					break;
				case 4:			//e
					c = 0.016709;
					m = -0.000000001151;
					break;
				case 5:			//M
					c = 356.0470;
					m = 0.9856002585;
					break;
			}
			break;
		case 1:
			switch (elementNum) {
				case 0:			//N
					c = 48.3313;
					m = 0.0000324587;
					break;
				case 1:			//i
					c = 7.0047;
					m = 0.00000005;
					break;
				case 2:			//w
					c = 29.1241;
					m = 0.0000101444;
					break;
				case 3:			//a
					c = 0.387098;
					m = 0;
					break;
				case 4:			//e
					c = 0.205635;
					m = 0.000000000559;
					break;
				case 5:			//M
					c = 168.6562;
					m = 4.0923344368;
					break;
			}
			break;
		case 2:
			switch (elementNum) {
				case 0:			//N
					c = 76.6799;
					m = 0.000024659;
					break;
				case 1:			//i
					c = 3.39;
					m = 0.0000000275;
					break;
				case 2:			//w
					c = 54.891;
					m = 0.0000138374;
					break;
				case 3:			//a
					c = 0.72333;
					m = 0;
					break;
				case 4:			//e
					c = 0.006773;
					m = -0.000000001302;
					break;
				case 5:			//M
					c = 48.0052;
					m = 1.6021302244;
					break;
			}
			break;
		case 3:
			switch (elementNum) {
				case 0:			//N
					c = 49.5574;
					m = 0.0000211081;
					break;
				case 1:			//i
					c = 1.8497;
					m = -0.0000000178;
					break;
				case 2:			//w
					c = 286.5016;
					m = 0.0000292961;
					break;
				case 3:			//a
					c = 1.523688;
					m = 0;
					break;
				case 4:			//e
					c = 0.093405;
					m = 0.000000002516;
					break;
				case 5:			//M
					c = 18.6021;
					m = 0.5240207766;
					break;
			}
			break;
		case 4:
			switch (elementNum) {
				case 0:			//N
					c = 100.4542;
					m = 0.0000276854;
					break;
				case 1:			//i
					c = 1.303;
					m = -0.0000001557;
					break;
				case 2:			//w
					c = 273.8777;
					m = 0.0000164505;
					break;
				case 3:			//a
					c = 5.20256;
					m = 0;
					break;
				case 4:			//e
					c = 0.048498;
					m = 0.000000004469;
					break;
				case 5:			//M
					c = 19.895;
					m = 0.0830853001;
					break;
			}
			break;
		case 5:
			switch (elementNum) {
				case 0:			//N
					c = 113.6634;
					m = 0.000023898;
					break;
				case 1:			//i
					c = 2.4886;
					m = -0.0000001081;
					break;
				case 2:			//w
					c = 339.3939;
					m = 0.0000297661;
					break;
				case 3:			//a
					c = 9.55475;
					m = 0;
					break;
				case 4:			//e
					c = 0.055546;
					m = -0.000000009499;
					break;
				case 5:			//M
					c = 316.967;
					m = 0.0334442282;
					break;
			}
			break;
		case 6:
			switch (elementNum) {
				case 0:			//N
					c = 74.0005;
					m = 0.000013978;
					break;
				case 1:			//i
					c = 0.7733;
					m = 0.000000019;
					break;
				case 2:			//w
					c = 96.6612;
					m = 0.000030565;
					break;
				case 3:			//a
					c = 19.18171;
					m = -0.0000000155;
					break;
				case 4:			//e
					c = 0.047318;
					m = 0.00000000745;
					break;
				case 5:			//M
					c = 142.5905;
					m = 0.011725806;
					break;
			}
			break;
		case 7:
			switch (elementNum) {
				case 0:			//N
					c = 131.7806;
					m = 0.000030173;
					break;
				case 1:			//i
					c = 1.77;
					m = -0.000000255;
					break;
				case 2:			//w
					c = 272.8461;
					m = -0.000006027;
					break;
				case 3:			//a
					c = 30.05826;
					m = 0.00000003313;
					break;
				case 4:			//e
					c = 0.008606;
					m = 0.00000000215;
					break;
				case 5:			//M
					c = 260.2471;
					m = 0.005995147;
					break;
			}
			break;
		default:
			APP_LOG(APP_LOG_LEVEL_ERROR, "Either the passed planet number or element number are wrong");
	}
	if (elementNum == 0 || elementNum == 2 || elementNum == 5) {
		return modDecimal(c + m * dDays, 360);
	} else {
		return (c + m * dDays);
	}
}			// Return the orbital element updated with current dDays
static void calculateGlobalVariables () {
	// Calculate sun characteristics first
	float w = returnOrbitalElement(0, 2);
	float a = returnOrbitalElement(0, 3) * 1000;
	float e = returnOrbitalElement(0, 4);
	float M = returnOrbitalElement(0, 5);
	float E = modDecimal(M + e * (180 / PIconst) * Sin(M) * (1.0 + e * Cos(M)), 360);
	float xv = a * (Cos(E) - e);
	float yv = a * (my_sqrt(1.0 - e * e) * Sin(E));
	float v = arctan2(yv, xv);
	r_sun = my_sqrt(xv * xv + yv * yv);
	E = v + w;		// Reappropriate E as lonsun
	xs = r_sun * Cos(E);
	ys = r_sun * Sin(E);

	// Next calculate the LST and GMST0
	e = modDecimal((float)(localTime - UToffset * 60) / 60, 24);		// Calculate the current UT time, Reappropriate e as UTtime
	GMST0 = modDecimal(w + M + 180, 360);
	LST = modDecimal(E + 180 + localLng_deg + e * 15, 360);

	// Next calculate the Ms and Mj
	M_jupiter = returnOrbitalElement(4, 5);
	M_saturn = returnOrbitalElement(5, 5);
}
static void calculate_RA_Decl (float* RA_calc, float* Decl_calc, float* rh_calc, float* rg_calc) {
	// Calculates the RA and Decl of a planet and return the values to a pointed address
	// First setup the orbital elements of the planet
	float temp1 = returnOrbitalElement(planetNumber, 0);			// temp1 is N
	float temp2 = returnOrbitalElement(planetNumber, 1);			// temp2 is i
	float temp3 = returnOrbitalElement(planetNumber, 2);			// temp3 is w
	float temp4 = returnOrbitalElement(planetNumber, 3) * 1000;			// temp4 is a by 1000
	float temp5 = returnOrbitalElement(planetNumber, 4);			// temp5 is e
	float temp6 = returnOrbitalElement(planetNumber, 5);			// temp6 is M

	//dispVal(temp1 * 10000, "N (4): ");
	//dispVal(temp2 * 10000, "i (4): ");
	//dispVal(temp3 * 10000, "w (4): ");
	//dispVal(temp4 * 10000, "a (4): ");
	//dispVal(temp5 * 10000, "e (4): ");
	//dispVal(temp6 * 10000, "M (4): ");

	// Calculate the planet's distance and true anomaly
	float temp7 = modDecimal(temp6 + temp5 * (180 / PIconst) * Sin(temp6) * (1.0 + temp5 * Cos(temp6)), 360);			// temp7 is E
	//dispVal(temp7 * 10000, "E (4): ");
	float temp8 = temp4 * (Cos(temp7) - temp5);													// temp8 is xv
	float temp9 = temp4 * (my_sqrt(1.0 - temp5 * temp5) * Sin(temp7));		// temp9 is yv
	temp7 = modDecimal(arctan2(temp9, temp8) + temp3, 360);				// temp7 is (v + w)
	*rh_calc = my_sqrt((temp8 * temp8) + (temp9 * temp9));

	//dispVal(temp8 * 10000, "xv (4): ");
	//dispVal(temp9 * 10000, "yv (4): ");
	//dispVal(temp7 * 10000, "v+w (4): ");
	//dispVal(*rh_calc * 10000, "rh (4): ");

	// Calculate the position of the planet in space
	temp3 = *rh_calc * (Cos(temp1) * Cos(temp7) - Sin(temp1) * Sin(temp7) * Cos(temp2));			// temp3 is xh
	temp4 = *rh_calc * (Sin(temp1) * Cos(temp7) + Cos(temp1) * Sin(temp7) * Cos(temp2));			// temp4 is yh
	temp5 = *rh_calc * (Sin(temp7) * Sin(temp2));			// temp5 is zh

	//dispVal(temp3 * 10000, "xh (4): ");
	//dispVal(temp4 * 10000, "yh (4): ");
	//dispVal(temp5 * 10000, "zh (4): ");

	// Now logic begins, need to calculate the longitudes and latitudes to be perturbed if on Jupiter, Saturn, or Uranus
	if (planetNumber == 4 || planetNumber == 5 || planetNumber == 6) {
		// Have to calculate the lonecl and latecl
		temp1 = arctan2(temp4, temp3);		// temp1 is lonecl
		temp2 = arctan2(temp5, my_sqrt(temp3 * temp3 + temp4 * temp4));			// temp2 is latecl

		//Determine the perturbations to be constructed
		switch (planetNumber) {
			case 4:
				// Adj lonecl
				temp1 = temp1 - 0.332 * Sin(2 * M_jupiter - 5 * M_saturn - 67.6);
        temp1 = temp1 - 0.056 * Sin(2 * M_jupiter - 2 * M_saturn + 21);
        temp1 = temp1 + 0.042 * Sin(3 * M_jupiter - 5 * M_saturn + 21);
        temp1 = temp1 - 0.036 * Sin(M_jupiter - 2 * M_saturn);
        temp1 = temp1 + 0.022 * Cos(M_jupiter - M_saturn);
        temp1 = temp1 + 0.023 * Sin(2 * M_jupiter - 3 * M_saturn + 52);
        temp1 = temp1 - 0.016 * Sin(M_jupiter - 5 * M_saturn - 69);
				break;
			case 5:
				// Adj lonecl
				temp1 = temp1 + 0.812 * Sin(2 * M_jupiter - 5 * M_saturn - 67.6);
        temp1 = temp1 - 0.229 * Cos(2 * M_jupiter - 4 * M_saturn - 2);
        temp1 = temp1 + 0.119 * Sin(M_jupiter - 2 * M_saturn - 3);
        temp1 = temp1 + 0.046 * Sin(2 * M_jupiter - 6 * M_saturn - 69);
        temp1 = temp1 + 0.014 * Sin(M_jupiter - 3 * M_saturn + 32);
				// Adj latecl
				temp2 = temp2 - 0.02 * Cos(2 * M_jupiter - 4 * M_saturn - 2);
        temp2 = temp2 + 0.018 * Sin(2 * M_jupiter - 6 * M_saturn - 49);
				break;
			case 6:
				// Adj lonecl
				temp1 = temp1 + 0.04 * Sin(M_saturn - 2 * temp6 + 6);
        temp1 = temp1 + 0.035 * Sin(M_saturn - 3 * temp6 + 33);
        temp1 = temp1 - 0.015 * Sin(M_jupiter - temp6 + 20);
				break;
		}

		// Revert back to xh, yh, and zh coords
		temp3 = *rh_calc * Cos(temp1) * Cos(temp2);
		temp4 = *rh_calc * Sin(temp1) * Cos(temp2);
		temp5 = *rh_calc * Sin(temp2);
	}

	// Convert to geocentric position, if planet is sun, this is already that position
	if (planetNumber != 0) {
		temp3 = temp3 + xs;
		temp4 = temp4 + ys;
	}

	//dispVal(temp3 * 10000, "xg (4): ");
	//dispVal(temp4 * 10000, "yg (4): ");
	//dispVal(temp5 * 10000, "zg (4): ");

	// Convert to equitorial coords
	temp1 = temp4 * Cos(ecl) - temp5 * Sin(ecl);		// temp1 is ye
	temp2 = temp4 * Sin(ecl) + temp5 * Cos(ecl);		// temp2 is ze

	//dispVal(temp3 * 10000, "xe (4): ");
	//dispVal(temp1 * 10000, "ye (4): ");
	//dispVal(temp2 * 10000, "ze (4): ");

	*RA_calc = arctan2(temp1, temp3);
	*Decl_calc = arctan2(temp2, my_sqrt(temp3 * temp3 + temp1 * temp1));
	*rg_calc = my_sqrt(temp1 * temp1 + temp2 * temp2 + temp3 * temp3);
}
static void calculateViewingData (int16_t* az, int16_t* alt, float RA_val, float Decl_val) {
	// Calculates the azimuth and altitude and passes them to assigned pointers
	float HA = LST - RA_val;
	float x = Cos(HA) * Cos(Decl_val);
	float yhor = Sin(HA) * Cos(Decl_val);
	float zhor = Sin(Decl_val);
	float xhor = x * Sin(localLat_deg) - zhor * Cos(localLat_deg);
	zhor = x * Cos(localLat_deg) + zhor * Sin(localLat_deg);

	*az = modDecimal(arctan2(yhor, xhor) + 180, 360);
	*alt = arctan2(zhor, my_sqrt(xhor * xhor + yhor * yhor));
	if (*alt > 180) {
		*alt = *alt - 360;		// Make altitude between -180 and 180
	}
}
static float calculateBrightness (float planetRh, float planetRg) {
	// Returns a planet's brightness
	float planet_brightness = 0;
	float FV = arccos((planetRh * planetRh + planetRg * planetRg - r_sun * r_sun) / (2 * planetRh * planetRg));			// Calculate the phase angle

	planetRh = planetRh / 1000;		// Transform the distances back to AU units
	planetRg = planetRg / 1000;

	switch (planetNumber) {
		case 0:
			planet_brightness = -26;		// Sun is the brightess object
			break;
		case 1:
			planet_brightness = -0.36 + 5 * logBase10(planetRh * planetRg) + 0.027 * FV + 0.00000000000022 * FV * FV * FV * FV * FV * FV;
			break;
		case 2:
			planet_brightness = -4.34 + 5 * logBase10(planetRh * planetRg) + 0.013 * FV + 0.00000042  * FV * FV * FV;
			break;
		case 3:
			planet_brightness = -1.51 + 5 * logBase10(planetRh * planetRg) + 0.016 * FV;
			break;
		case 4:
			planet_brightness = -9.25 + 5 * logBase10(planetRh * planetRg) + 0.014 * FV;
			break;
		case 5:
			planet_brightness = -9.0  + 5 * logBase10(planetRh * planetRg) + 0.044 * FV;
			break;
		case 6:
			planet_brightness = -7.15 + 5 * logBase10(planetRh * planetRg) + 0.001 * FV;
			break;
		case 7:
			planet_brightness = -6.90 + 5 * logBase10(planetRh * planetRg) + 0.001 * FV;
			break;
		default:
			planet_brightness = 101;	// Error value of brightness
	}
	return planet_brightness;
}
static void calculateRiseSetTimes (float planetRA, float planetDecl, int16_t* planetRise, int16_t* planetSet) {
	// Calculate the rise and set time for a planet
	float h = 360-0.833;		// Height trigger for rise or set
	float UT_in_south = ((planetRA - GMST0 - localLng_deg) / 15);
	float LHA = (Sin(h) - Sin(localLat_deg) * Sin(planetDecl)) / (Cos(localLat_deg) * Cos(planetDecl));

	if (LHA > 1) {
		*planetRise = 3000;			// Error code for no rise
		*planetSet = 3000;
	} else if (LHA < -1) {
		*planetRise = 4000;			// Error code for no set
		*planetSet = 4000;
	} else {
		LHA = arccos(LHA) / 15;
		*planetRise = modDecimal(UT_in_south - LHA + UToffset, 24) * 60;
		*planetSet = modDecimal(UT_in_south + LHA + UToffset, 24) * 60;
	}
}

// Display updating functions ------------------------------------------------------------------------------------------------------------------------------------------------
static int returnMinTens (int timeCode) {
  return (timeCode/10) % 10;
}
static int returnMinOnes (int timeCode) {
  return timeCode % 10;
}
static void decodeDateCode (int32_t dateInCodeFormat, int16_t* yearVar, int8_t* monthVar, int8_t* dayVar) {
	*yearVar = dateInCodeFormat / 10000;
  *monthVar = (dateInCodeFormat % 10000) / 100;
  *dayVar = dateInCodeFormat % 100;
}
static void updateCoordsDispLayer(int GPSstatusToDisplay, int16_t passedAngle) {
	static char coordBuffer[50] = "No GPS data in memory";
	int8_t dayVar, monthVar;
	int16_t yearVar;

	switch (GPSstatusToDisplay) {
		case GPS_AWAITING_DATA:
			strcpy(coordBuffer, "Awaiting GPS data");
			break;
		case DISPLAY_COORDS:
			decodeDateCode(lastUpdateDate, &yearVar, &monthVar, &dayVar);
			if (lastUpdateDate != 0) {
				//Update the location data
  			snprintf(coordBuffer, sizeof(coordBuffer), "%d-%d-%d %d:%d%d UTC%d\nLat %d.%d%d, Long %d.%d%d", dayVar, monthVar, yearVar, lastUpdateTime / 60, returnMinTens(lastUpdateTime % 60), returnMinOnes(lastUpdateTime % 60), (int)UToffset, (int)localLat_deg, returnMinTens(Abs(localLat_deg) * 100), returnMinOnes(Abs(localLat_deg) * 100), (int)localLng_deg, returnMinTens(Abs(localLng_deg) * 100), returnMinOnes(Abs(localLng_deg) * 100));
			} else {
				strcpy (coordBuffer, "No GPS data in memory");
			}
			break;
		case DISPLAY_COMPASS:
		case DISPLAY_INCLINATION:
			if (passedAngle == 400) {
				strcpy(coordBuffer, "Not ready");
			} else if (GPSstatusToDisplay == DISPLAY_COMPASS) {
				snprintf(coordBuffer, sizeof(coordBuffer), "az %d deg", passedAngle);
			} else {
				snprintf(coordBuffer, sizeof(coordBuffer), "alt %d deg", passedAngle);
			}
			break;
		default:
			APP_LOG(APP_LOG_LEVEL_ERROR, "Incorrect display state passed to updateCoordsDispLayer");
	}
	text_layer_set_text(s_lastCoord_layer, coordBuffer);
}
static void updateAstronomyData_planetScreen (int16_t alt, int16_t az, int16_t riseTime, int16_t setTime, float brightMag) {
	// Updates the data shown on the planet screen
	strcpy(globalBuffer[0], "Altitude\n\n\n\nRise");
	strcpy(globalBuffer[1], "Azimuth\n\n\n\nSet");
	strcpy(globalBuffer[2], "na");
	strcpy(globalBuffer[3], "na");
	strcpy(globalBuffer[4], "Brightness: na");

	if (lastUpdateDate != 0) {
		snprintf(globalBuffer[2], sizeof(globalBuffer[2]), "%d\n%d:%d%d", alt, riseTime / 60, returnMinTens(riseTime % 60), returnMinOnes(riseTime % 60));
		snprintf(globalBuffer[3], sizeof(globalBuffer[3]), "%d\n%d:%d%d", az, setTime / 60, returnMinTens(setTime % 60), returnMinOnes(setTime %60));
		snprintf(globalBuffer[4], sizeof(globalBuffer[4]), "Brightness: %d.%d", (int)brightMag, returnMinOnes(Abs(brightMag) * 10));
	}

	// Display the data
	text_layer_set_text(s_belowCoord_layer, planetNamesArr[planetNumber]);
	text_layer_set_text(s_genericText1_layer, globalBuffer[0]);
	text_layer_set_text(s_genericText2_layer, globalBuffer[2]);
	text_layer_set_text(s_genericText4_layer, globalBuffer[1]);
	text_layer_set_text(s_genericText5_layer, globalBuffer[3]);
	text_layer_set_text(s_genericText3_layer, globalBuffer[4]);
}
static void highlightBrightnessSelection () {
	// Inverts the planet layer dependent on the planet number
	switch (planetNumber) {
		case 0:
			APP_LOG(APP_LOG_LEVEL_ERROR, "The sun is selected, however brightness display does not show sun settings");
			break;
		case 1:
			switchHighlightLayer(s_genericText6_layer, s_belowCoord_layer, s_genericText1_layer);
			break;
		case 2:
			switchHighlightLayer(s_belowCoord_layer, s_genericText1_layer, s_genericText2_layer);
			break;
		case 3:
			switchHighlightLayer(s_genericText1_layer, s_genericText2_layer, s_genericText3_layer);
			break;
		case 4:
			switchHighlightLayer(s_genericText2_layer, s_genericText3_layer, s_genericText4_layer);
			break;
		case 5:
			switchHighlightLayer(s_genericText3_layer, s_genericText4_layer, s_genericText5_layer);
			break;
		case 6:
			switchHighlightLayer(s_genericText4_layer, s_genericText5_layer, s_genericText6_layer);
			break;
		case 7:
			switchHighlightLayer(s_genericText5_layer, s_genericText6_layer, s_belowCoord_layer);
			break;
		default:
			APP_LOG(APP_LOG_LEVEL_ERROR, "Passed planet number does not match the optional cases");
	}
}

// Calculation and display handlers for the astronomical data --------------------------------------------------------------------------------------------------------------
static void calculateTimeAstroData() {
	// Astro calculations that pertain to an update in time and date data
	// Use current time
	// Get a tm structure
	time_t temp = time(NULL);
  struct tm *tick_time = localtime(&temp);
	localTime = tick_time->tm_hour * 60 + tick_time->tm_min;
	cCodeDate = (tick_time->tm_year + 1900) * 10000 + (tick_time->tm_mon + 1) * 100 + tick_time->tm_mday;
	dDays = calc_d_JDate(cCodeDate, localTime, UToffset);			// Calculate the current dDays
	ecl = 23.4393 - 0.0000003563 * dDays;													// With dDays updated, may as well update ecl
	// First the global LST, xs, ys, r_sun, GMST0, M_jupiter, and M_saturn must be calculated as they may be called and are the same for all planets
	calculateGlobalVariables();
}
static void populateAstronomyData(bool brightnessScreen) {
	// Second, calculate RA, Decl, rh, and rg of planet
	float RA, Decl, rh, rg, calcBright;
	if (brightnessScreen) {
		// Step through each planet and find the brightness
		for (planetNumber = 1; planetNumber <= 7; planetNumber++) {
			calculate_RA_Decl(&RA, &Decl, &rh, &rg);		// Second, calculate RA, Decl, rh, and rg of planet
			calcBright = calculateBrightness(rh, rg);		// I want to know the brightness of the planet!
			// Fill the buffers
			if (lastUpdateDate == 0) {
				snprintf(globalBuffer[planetNumber - 1], sizeof(globalBuffer[planetNumber - 1]), "%s      na", planetNamesArr[planetNumber]);
			} else {
				if (calcBright < 4) {
					snprintf(globalBuffer[planetNumber - 1], sizeof(globalBuffer[planetNumber - 1]), "%s      %d.%d    V", planetNamesArr[planetNumber], (int)calcBright, returnMinOnes(Abs(calcBright) * 10));
				} else {
					snprintf(globalBuffer[planetNumber - 1], sizeof(globalBuffer[planetNumber - 1]), "%s      %d.%d", planetNamesArr[planetNumber], (int)calcBright, returnMinOnes(Abs(calcBright) * 10));
				}
			}
		}
		// Display the data
		text_layer_set_text(s_belowCoord_layer, globalBuffer[0]);
		text_layer_set_text(s_genericText1_layer, globalBuffer[1]);
		text_layer_set_text(s_genericText2_layer, globalBuffer[2]);
		text_layer_set_text(s_genericText3_layer, globalBuffer[3]);
		text_layer_set_text(s_genericText4_layer, globalBuffer[4]);
		text_layer_set_text(s_genericText5_layer, globalBuffer[5]);
		text_layer_set_text(s_genericText6_layer, globalBuffer[6]);
	} else {
		calculate_RA_Decl(&RA, &Decl, &rh, &rg);		// Second, calculate RA, Decl, rh, and rg of planet
		calcBright = calculateBrightness(rh, rg);		// I want to know the brightness of the planet!
		// Next calculate the azimuth and altitude
		int16_t azimuth, altitude;
		calculateViewingData(&azimuth, &altitude, RA, Decl);
		// Now I want to know the set and rise times
		int16_t riseTime, setTime;
		calculateRiseSetTimes(RA, Decl, &riseTime, &setTime);
		// Create the text buffer and display
		updateAstronomyData_planetScreen(altitude, azimuth, riseTime, setTime, calcBright);
	}
}

// App communication with phone for GPS requests ----------------------------------------------------------------------------------------------------------------------------
static void inbox_received_callback(DictionaryIterator *iterator, void *context) {
	//APP_LOG(APP_LOG_LEVEL_INFO, "Got data back from Phone");
  // This is the main function that operates when a callback happens for the GPS request
  // Store incoming information
  int16_t newLatitude;
  int16_t newLongitude;
  int8_t useNewData = 0;		//Assume using old data until proven otherwise
  int8_t newUToffset;

  // Read first item
  Tuple *t;
	t = dict_read_first(iterator);

  // For all items
  while(t != NULL) {
    // Which key was received?
    switch(t->key) {
			case 0:
				// Key 0 is the first entry in the dictionary, skipped for values as I believe that was the source of problems..
				break;
      case KEY_LATITUDE:
        newLatitude = t->value->int32;
        break;
      case KEY_LONGITUDE:
        newLongitude = (int)t->value->int32;
        break;
      case KEY_USEOLDDATA:
        useNewData = (int)t->value->int32;
        break;
      case KEY_UTCh:
        newUToffset = (int)t->value->int32;
        break;
      default:
        APP_LOG(APP_LOG_LEVEL_ERROR, "Key %d not recognized!", (int)t->key);
    }

    // Look for next item
    t = dict_read_next(iterator);
  }		//Transer the incoming information into the stores

  // Set the latitude and longitude global variables
  if (useNewData == 0) {
    // No new position data available, use old data
    // Latitude and longitude remain unchanged (along with time zone I believe)
		//APP_LOG(APP_LOG_LEVEL_INFO, "Data invalid from Phone");
		/*
		if (GPScalls < 3) {
			//Flag for a re-ping, dictionary memory will be destroyed, but callbacks are still registered
			repingGPS = 1;
		} else {
			//Run an update on the, as data has been correctly received and processed
			repingGPS = 0;
			moonModule_deinit();		//Call to deregister the callbacks (no longer needed)
			update_Astronomy();			//Refresh calculations
		}
		*/

  } else {
    // New position data available, update global variables. Lat, long, and UTC must remain as flexible variables, Rise values can be directly accessed through storage
    // Last update time can be updated to now as well
		//APP_LOG(APP_LOG_LEVEL_INFO, "Data valid from Phone");
    time_t localSeconds = time(NULL);
    struct tm *tick_time = localtime(&localSeconds);
    lastUpdateTime = (tick_time->tm_hour) * 60 + (tick_time->tm_min);
    lastUpdateDate = (tick_time->tm_year + 1900) * 10000 + (tick_time->tm_mon + 1) * 100 + tick_time->tm_mday;    //Alterations account for time.h functions

    localLat_deg = (float)newLatitude/100;
    localLng_deg = (float)newLongitude/100;
    UToffset = newUToffset;    //Already in hrs
    persist_write_int(LAT_STORED, newLatitude);    //Already as an integer (must / 100 when retrieving)
    persist_write_int(LNG_STORED, newLongitude);
    persist_write_int(UTC_STORED, newUToffset);
		persist_write_int(LAST_TIME_STORED, lastUpdateTime);
		persist_write_int(LAST_DATE_STORED, lastUpdateDate);
  }
	//Update the displays of GPS data, as data has been correctly received and processed
	calculateTimeAstroData();
	updateCoordsDispLayer(DISPLAY_COORDS, 0);		// Pass arbitary angle value through, it won't be used
}
static void inbox_dropped_callback(AppMessageResult reason, void *context) {
  //APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped!");
	updateCoordsDispLayer(DISPLAY_COORDS, 0);		//Redisplay values
}
static void outbox_failed_callback(DictionaryIterator *iterator, AppMessageResult reason, void *context) {
  //APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox send failed!");
	updateCoordsDispLayer(DISPLAY_COORDS, 0);		//Redisplay values
}
static void outbox_sent_callback(DictionaryIterator *iterator, void *context) {
  //APP_LOG(APP_LOG_LEVEL_INFO, "Outbox send success!");
}
static void update_GPS() {
	//APP_LOG(APP_LOG_LEVEL_INFO, "Entered update_GPS()");
	updateCoordsDispLayer(GPS_AWAITING_DATA, 0);		//Redisplay values

	// First increment the number of calls by setting GPScalls to the seed value passed through
	//GPScalls = nCalls + 1;
  // Begin dictionary
  DictionaryIterator *iter;
  app_message_outbox_begin(&iter);
  // Add a key-value pair
  dict_write_uint8(iter, 0, 0);
  // Send the message!
  app_message_outbox_send();
}

// Service handlers, Accelerometer and Compass used --------------------------------------------------------------------------------------------------------------------------
static void compass_callback(CompassHeadingData heading) {
  if (heading.compass_status != CompassStatusDataInvalid) {
    updateCoordsDispLayer(DISPLAY_COMPASS, (360 - TRIGANGLE_TO_DEG((int)heading.true_heading)));
		if (heading.is_declination_valid) {
			bitmap_layer_set_bitmap(s_compassType_layer, s_trueNorth_icon);
		} else {
			bitmap_layer_set_bitmap(s_compassType_layer, s_magNorth_icon);
		}
  } else {
    // Heading not available yet - Show calibration UI to user
    updateCoordsDispLayer(DISPLAY_COMPASS, 400);		// Display error value
  }
}			// Compass service handler, displaying compass bearing on the coord header
static void accel_data_handler(AccelData *data, uint32_t num_samples) {
	// The handler calculates the inclination with trigonometry, using an average of samples
	float yG = 0;
	float zG = 0;
	for (uint8_t i = 0; i < num_samples; i++) {
		yG = yG + data[i].y;
		zG = zG + data[i].z;
	}
	// Average the samples
	yG = (yG / num_samples);
	zG = (zG / num_samples);		// The g force has to be increase by gravity

	// Do the trig to get an angle, the zG is the y axis, and yG is z axis, since adjusted for gravity should give a positive angle - pass straight to the update display
	updateCoordsDispLayer(DISPLAY_INCLINATION, 270 - arctan2(zG, yG));
}		// Inclination service handler, displaying the inclination on the coord header
static void time_tick_handler(struct tm *tick_time, TimeUnits units_changed) {
	// If on the planet window, update the displayed values, this will occur every minute
	calculateTimeAstroData();
	if (currentWindow == WINDOWnum_PLANET) {
		populateAstronomyData(false);
	}
}

// Button press handlers ------------------------------------------------------------------------------------------------------------------------------------------------------
static void up_click_handler (ClickRecognizerRef recognizer, void *context) {
	switch (currentWindow) {
		case WINDOWnum_START:
			// Do nothing
			break;
		case WINDOWnum_PLANET:
			// Switch the display of coords, compass, inclination
			switch (currentHeader) {
				case DISPLAY_COORDS:
					// Currently displaying the coordinates, move to the compass display
					updateCoordsDispLayer(DISPLAY_COMPASS, 400);
					text_layer_set_font(s_lastCoord_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28));		// Enlarge the font
					compass_service_subscribe(compass_callback);			// Subscribe to the compass service
  				compass_service_set_heading_filter(TRIG_MAX_ANGLE / 180);    //Only update the compass if the heading changes more than 2 degrees
					light_enable(true);		// Turn on the light for the compass and inclination
					currentHeader = DISPLAY_COMPASS;		// Now showing the compass
					break;
				case DISPLAY_COMPASS:
					// Currently displaying the compass, move to the incliniation display
					updateCoordsDispLayer(DISPLAY_COMPASS, 400);
					compass_service_unsubscribe();			// Unsubscribe from the compass service
					bitmap_layer_set_bitmap(s_compassType_layer, s_emptyNorth_icon);
					accel_data_service_subscribe(5, accel_data_handler);		// Subscribe to the accelerometer service
					accel_service_set_sampling_rate(ACCEL_SAMPLING_25HZ);		// Set sampling rate to lowest setting
					currentHeader = DISPLAY_INCLINATION;			// Now showing the inclination
					break;
				case DISPLAY_INCLINATION:
					// Currently displaying the inclination, move to the coords display
					accel_data_service_unsubscribe();	// Unsubscribe from the inclination service
					text_layer_set_font(s_lastCoord_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));		// De-enlarge the font
					light_enable(false);		// Turn off the light
					currentHeader = DISPLAY_COORDS;			// Now showing the coordinates
					updateCoordsDispLayer(DISPLAY_COORDS, 0);
					break;
				default:
					APP_LOG(APP_LOG_LEVEL_ERROR, "Wrong currentHeader value passed");
			}
			break;
		case WINDOWnum_MENU:
			// Move to the brightness window
			window_stack_push(s_brightness_window, true);
			populateAstronomyData(true);		// Calculate the brightnesses
			planetNumber = 1;		// Set the brightness selector on Mercury
			highlightBrightnessSelection();
			break;
		case WINDOWnum_BRIGHTNESS:
			// Move the planet brightness selection levels
			if (planetNumber == 0 || planetNumber == 1) {
				planetNumber = 7;
			} else {
				planetNumber--;
			}
			highlightBrightnessSelection();		// Update the selection
			break;
		default:
			APP_LOG(APP_LOG_LEVEL_ERROR, "On a window that does not have click handling, but button was clicked");
	}
}
static void middle_click_handler (ClickRecognizerRef recognizer, void *context) {
	switch (currentWindow) {
		case WINDOWnum_START:
			// Update GPS was selected
			update_GPS();
			window_stack_push(s_planet_window, true);			// Just get off this screen
			populateAstronomyData(false);
			break;
		case WINDOWnum_PLANET:
			// Increment and decrement the planet number
			if (planetNumber == 0) {
				planetNumber = 7;
			} else {
				planetNumber--;
			}
			populateAstronomyData(false);
			break;
		case WINDOWnum_MENU:
			update_GPS();		// Update GPS was selected
			window_stack_pop_all(false);		// Instead of popping just the menu window, pop all and push the planet window. This fixes a crashing issue when starting a new version
			window_stack_push(s_planet_window, true);
			populateAstronomyData(false);		// Update the values
			break;
		case WINDOWnum_BRIGHTNESS:
			// Move to the planet screen, using the selected planet number
			window_stack_pop_all(false);		// Popping all is more efficient than individually stepping back to the planet window.
			window_stack_push(s_planet_window, true);
			populateAstronomyData(false);
			break;
		default:
			APP_LOG(APP_LOG_LEVEL_ERROR, "On a window that does not have click handling, but button was clicked");
	}
}
static void down_click_handler (ClickRecognizerRef recognizer, void *context) {
	switch (currentWindow) {
		case WINDOWnum_START:
			// No GPS update needed
			window_stack_push(s_planet_window, true);			// Just get off this screen
			populateAstronomyData(false);
			break;
		case WINDOWnum_PLANET:
			// Increment and decrement the planet number
			if (planetNumber == 7) {
				planetNumber = 0;
			} else {
				planetNumber++;
			}
			populateAstronomyData(false);
			break;
		case WINDOWnum_MENU:
			text_layer_set_font(s_belowCoord_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
			text_layer_set_text_alignment(s_belowCoord_layer, GTextAlignmentLeft);
			setAboutBuffer();
			text_layer_set_text(s_belowCoord_layer, aboutBuffer);
			break;
		case WINDOWnum_BRIGHTNESS:
			// Move the planet brightness selection levels
			if (planetNumber == 0 || planetNumber == 7) {
				planetNumber = 1;
			} else {
				planetNumber++;
			}
			highlightBrightnessSelection();		// Update the selection
			break;
		default:
			APP_LOG(APP_LOG_LEVEL_ERROR, "On a window that does not have click handling, but button was clicked");
	}
}
static void return_click_handler (ClickRecognizerRef recognizer, void *context) {
	switch (currentWindow) {
		case WINDOWnum_START:
			window_stack_pop_all(true);		// Exit exit from start screen
			break;
		case WINDOWnum_PLANET:
			window_stack_push(s_menu_window, true);		// Display the menu window
			break;
		case WINDOWnum_MENU:
			window_stack_pop_all(true);		// Exit app from menu
			break;
		case WINDOWnum_BRIGHTNESS:
			window_stack_pop(true);		// Return to the menu window
			break;
		default:
			APP_LOG(APP_LOG_LEVEL_ERROR, "Return clicked on a window that does not have a handler");
	}
}
static void click_config_provider (void *context) {
  // Register the ClickHandlers
  window_single_click_subscribe(BUTTON_ID_UP, up_click_handler);
	window_single_click_subscribe(BUTTON_ID_SELECT, middle_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click_handler);
	window_single_click_subscribe(BUTTON_ID_BACK, return_click_handler);
}

// App function handling -----------------------------------------------------------------------------------------------------------------------------------------------------
static void typicalWindowLayer(Window *window) {
	window_set_background_color(window, GColorBlack);		// Set background color as black
	window_set_click_config_provider(window, click_config_provider);
}
static void init() {
	// Called when first initialising the app
	// Holds any persistent layers, as well as creating the different windows

	// Create the constants, initialise global values
	planetNumber = 0;
	PIconst = 3.14159265359;		// Set the global constant of pi
	planetNamesArr[0] = "Sun";
	planetNamesArr[1] = "Mercury";
	planetNamesArr[2] = "Venus";
	planetNamesArr[3] = "Mars";
	planetNamesArr[4] = "Jupiter";
	planetNamesArr[5] = "Saturn";
	planetNamesArr[6] = "Uranus";
	planetNamesArr[7] = "Neptune";
	aboutBuffer = (char*)malloc(1);

	// Register the callbacks for when a GPS request is made
	app_message_register_inbox_received(inbox_received_callback);
  app_message_register_inbox_dropped(inbox_dropped_callback);
  app_message_register_outbox_failed(outbox_failed_callback);
  app_message_register_outbox_sent(outbox_sent_callback);
	// Open AppMessage
  app_message_open(APP_MESSAGE_INBOX_SIZE_MINIMUM, APP_MESSAGE_OUTBOX_SIZE_MINIMUM);		//Hopefully the guaranteed size is large enough to carry the data across bluetooth

	//APP_LOG(APP_LOG_LEVEL_INFO, "About to access persistent data");
	// Initialise some of the global values
  localLat_deg = (float)persist_read_int(LAT_STORED) / 100;
  localLng_deg = (float)persist_read_int(LNG_STORED) / 100;
  UToffset = (int8_t)persist_read_int(UTC_STORED);
  lastUpdateDate = (int32_t)persist_read_int(LAST_DATE_STORED);
	lastUpdateTime = (int16_t)persist_read_int(LAST_TIME_STORED);
	currentHeader = DISPLAY_COORDS;

	//APP_LOG(APP_LOG_LEVEL_INFO, "About to create persistent layers");
	// Create the persisent layers
	// Create the GPS coord layer which exists in two different windows	--> Create2
	s_lastCoord_layer = text_layer_create(GRect(0, 0, 133, 35));
	typicalTextLayer(s_lastCoord_layer, 2);
	//APP_LOG(APP_LOG_LEVEL_INFO, "Returned from typicalTextLayer() - about to change the font");
	text_layer_set_font(s_lastCoord_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));

	//APP_LOG(APP_LOG_LEVEL_INFO, "About to fill the coords layer");
	// Fill the GPS coord layer with last known data
	updateCoordsDispLayer(DISPLAY_COORDS, 0);

	//APP_LOG(APP_LOG_LEVEL_INFO, "About to create the windows");
	// Create the different windows
	s_start_window = window_create();		// Create the starting screen window --> Create1
	typicalWindowLayer(s_start_window);
	window_set_window_handlers(s_start_window, (WindowHandlers) {
		.appear = start_window_appear,
		.disappear = start_window_disappear
	});		// Set the start window handlers

	s_planet_window = window_create();		// Create the planet screen window --> Create8
	typicalWindowLayer(s_planet_window);
	window_set_window_handlers(s_planet_window, (WindowHandlers) {
		.appear = planet_window_appear,
		.disappear = planet_window_disappear
	});		// Set the start window handlers

	s_menu_window = window_create();		// Create the planet screen window --> Create12
	typicalWindowLayer(s_menu_window);
	window_set_window_handlers(s_menu_window, (WindowHandlers) {
		.appear = menu_window_appear,
		.disappear = menu_window_disappear
	});		// Set the start window handlers

	s_brightness_window = window_create();		// Create the planet screen window --> Create20
	typicalWindowLayer(s_brightness_window);
	window_set_window_handlers(s_brightness_window, (WindowHandlers) {
		.appear = brightness_window_appear,
		.disappear = brightness_window_disappear
	});		// Set the start window handlers


	if (persist_read_int(VERSION_NUMBER) != BUILD_NUMBER) {
		// A new version has been released, show the about screen
		window_stack_push(s_menu_window, true);		// Show menu window, then force the about selection
		text_layer_set_font(s_belowCoord_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
		text_layer_set_text_alignment(s_belowCoord_layer, GTextAlignmentLeft);
		setAboutBuffer();
		text_layer_set_text(s_belowCoord_layer, aboutBuffer);
		persist_write_int(VERSION_NUMBER, BUILD_NUMBER);
	} else {
		// Display the start window
		window_stack_push(s_start_window, true);
	}

	calculateTimeAstroData();		// Initialise the global variables for the current time
	// Register to the time tick service
	tick_timer_service_subscribe(MINUTE_UNIT, time_tick_handler);
}
static void deinit() {
	// Destroy any subscriptions, windows, and persistent layers
	// Destroy subscriptions
	tick_timer_service_unsubscribe();				// Destroy the timer callbacks first to avoid unwarranted calling
	app_message_deregister_callbacks();    //Destroy the callbacks for clean up
	//Destroy the persistent layers
	text_layer_destroy(s_lastCoord_layer);		// Destroy the last coords text layer --> Create2
	// Destroy the windows
	window_destroy(s_brightness_window);		// Destroy the brightness window --> Create20
	window_destroy(s_menu_window);		// Destroy the menu window --> Create12
	window_destroy(s_planet_window);	// Destroy the planet window --> Create8
	window_destroy(s_start_window);		// Destroy the start window --> Create1
}
int main(void) {
	init();
	app_event_loop();
	deinit();
}
