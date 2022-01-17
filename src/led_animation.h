//	FUNCTION-BASED ANIMATION SEQUENCES FOR WS2812 LED STRIPS
//	Using FastLED library
//  	Author: Dave Morris:	http://www.google.com/+DaveMorris128
//	Version 1.0 (2014-07-31)
//
//
//  	The following code includes "primitive animations" which are the base effect and
//	"aggregate animations" which are combinations of one or more primitive animations
//	Feel free to combine different primitives each loop for synergistic results but:
//		-If using an aggregate animation make sure your primatives don't clear the buffer each frame (FastLED.clear())
//		otherwise one animation will clear out any upstream animations before the loop sends the frame (FastLED.show())		
//	This code is designed to handle multiple LED strips, each with its own animation....
//  e.g.
//		void loop()
//		{
//			Ring(stripA, frame, 30);
//			Spark(stripB, frame, 245);
//			FastLED.show();
//			frame += animateSpeed;
//		}
	

#include "FastLED.h"

#define NUM_LEDS 12    	//Length of your LED strip
#define MAX_INT_VALUE 65536
#define MAX_BRIGHTNESS 255 // watch the power!

CRGB led_strip[NUM_LEDS+1];

unsigned long animationStartTime = 0;
uint16_t frame = 0;			//I think I might be able to move this variable to the void loop() scope and save some CPU
uint16_t animateSpeed = 100;          	//Number of frames to increment per loop
uint8_t  animation = 0;		//Active animation
uint8_t brightness = 50;		//Global brightness percentage
CRGB animateRGB = CRGB::Black;
CHSV animateHSV;

CRGB decode_color_crgb(String color) {
    if (color == "black") return CRGB::Black;
    if (color == "white") return CRGB::White;
    if (color == "red") return CRGB::Red;
    if (color == "green") return CRGB::Green;
    if (color == "blue") return CRGB::Blue;
    if (color == "yellow") return CRGB::Yellow;
    return CRGB::Black;
}

void led_animation_stop() {
	animation = 0;
}

void led_animation(uint8_t animation_n, uint8_t speed, const char *color) {
	led_animation_stop();
	animation = animation_n;
	animateSpeed = speed;
	animateRGB = decode_color_crgb(color);
	animateHSV = rgb2hsv_approximate(animateRGB);
	animationStartTime = millis();
}

void led_animation_setup() {
	FastLED.addLeds<NEOPIXEL, 15>(led_strip, NUM_LEDS);
	FastLED.setBrightness(MAX_BRIGHTNESS);
 	FastLED.clear();

	animateRGB = CRGB::Black;
	animateHSV = rgb2hsv_approximate(animateRGB);
	led_animation_stop();
}

//#######################################################################################################
//##                                PRIMATIVE ANIMATION FUNCTIONS                                      ##
//#######################################################################################################

//Anti-aliasing code care of Mark Kriegsman Google+: https://plus.google.com/112916219338292742137/posts/2VYNQgD38Pw
void drawFractionalBar(CRGB targetStrip[], int pos16, int width, uint8_t hue, bool wrap)
{
	uint8_t stripLength = sizeof(led_strip)/sizeof(CRGB);
	uint8_t i = pos16 / 16; // convert from pos to raw pixel number

	uint8_t frac = pos16 & 0x0F; // extract the 'factional' part of the position
	uint8_t firstpixelbrightness = 255 - (frac * 16);
	
	uint8_t lastpixelbrightness = 255 - firstpixelbrightness;

	uint8_t bright;
	for (int n = 0; n <= width; n++) {
		if (n == 0) {
			// first pixel in the bar
			bright = firstpixelbrightness;
		}
		else if (n == width) {
			// last pixel in the bar
			bright = lastpixelbrightness;
		}
		else {
			// middle pixels
			bright = 255;
		}

		// targetStrip[i] += CHSV(hue, 255, bright );
		// targetStrip[i] += animateHSV;
		targetStrip[i] += CHSV(animateHSV.hue, animateHSV.saturation, bright);
		i++;
		if (i == stripLength)
		{
			if (wrap == 1) {
				i = 0; // wrap around
			}
			else{
				return;
			}
		}
	}
}

//*********************     Bounce      ***************************
// Linear "Larson scanner" (or knight rider effect) with anti-aliasing
// Color is determined by "hue"
//*****************************************************************
void Bounce(CRGB targetStrip[], uint16_t animationFrame, uint8_t hue)
{
	uint16_t pos16;
	if (animationFrame < (MAX_INT_VALUE / 2))
	{
		pos16 = animationFrame * 2;
	
	}else{
		pos16 = MAX_INT_VALUE - ((animationFrame - (MAX_INT_VALUE/2))*2);
	}

	int position = map(pos16, 0, MAX_INT_VALUE, 0, ((NUM_LEDS) * 16));
	drawFractionalBar(targetStrip, position, 3, hue,0);
}

//************************          Ring           ******************************
// Anti-aliased cyclical chaser, 3 pixels wide
// Color is determined by "hue"
//*****************************************************
void Ring(CRGB targetStrip[], uint16_t animationFrame, uint8_t hue)
{
	uint8_t stripLength = sizeof(led_strip)/sizeof(CRGB);
	int pos16 = map(animationFrame, 0, MAX_INT_VALUE, 0, ((stripLength) * 16));
	drawFractionalBar(targetStrip, pos16, 3, hue,1);
}

//***************************   Wave [Float Math]  *******************************
// Squeezed sine wave  
// Uses slow, Arduino sin() function
// Squeezing achieved by using an exponential (^8) sin value
// Color is determined by "hue"
//***********************************************************************************
void Wave(CRGB targetStrip[], uint16_t animationFrame, uint8_t hue){
	FastLED.clear();		//Clear previous buffer
	float deg; 
	float value; 
	uint8_t stripLength = sizeof(led_strip)/sizeof(CRGB);
	for(uint8_t i=0;i<stripLength;i++)
	{
		deg=float(animationFrame+((MAX_INT_VALUE/stripLength)*i))/(float(MAX_INT_VALUE)) * 360.0;
		value = pow(sin(radians(deg)),8);    //Squeeeeeeze

		if(value>=0){		//Chop sine wave (no negative values)
			targetStrip[i] += CHSV(hue,255,value*256);
		}
	}	
}


//***************************   Wave [Integer Math]  *******************************
// unadulterated sine wave.  
// Uses FastLED sin16() and no float math for efficiency. 
// Since im stuck with integer values, exponential wave-forming is not possible (unless i'm wrong???)
// Color is determined by "hue"
//***********************************************************************************
void WaveInt(CRGB targetStrip[], uint16_t animationFrame, uint8_t hue){
	FastLED.clear();
	uint8_t stripLength = sizeof(led_strip)/sizeof(CRGB);
	uint8_t value;
	for(uint8_t i=0;i<stripLength;i++)
	{
		value=(sin16(animationFrame+((MAX_INT_VALUE/stripLength)*i)) + (MAX_INT_VALUE/2))/256;   
		if(value>=0){   
			targetStrip[i] += CHSV(hue,255,value);
		}
	}	
}

//********************************   Color Spark  ***********************************
// Color of the sparks is determined by "hue"
// Frequency of sparks is determined by global var "animateSpeed"
// "animateSpeed" var contrained from 1 - 255 (0.4% - 100%)
// "fade" parameter specifies dropoff (next frame brightness = current frame brightness * (x/256)
// fade = 256 means no dropoff, pixels are on or off
// NOTE: this animation doesnt clear the previous buffer because the fade/dropoff is a function of the previous LED state
//***********************************************************************************
void Spark(CRGB targetStrip[], uint16_t animationFrame,uint8_t fade, uint8_t hue){

	uint8_t stripLength = sizeof(led_strip)/sizeof(CRGB);
	uint16_t rand = random16();

		for(int i=0;i<stripLength;i++)
		{		
			targetStrip[i].nscale8(fade);
		}


	if(rand < (MAX_INT_VALUE / (256 - (constrain(animateSpeed,1,256)))))	;
	{
		targetStrip[rand % stripLength].setHSV(hue,255,255);
	}
}


//******************************       Spark       **********************************
// Same as color spark but no hue value, // in HSV white is any hue with 0 saturation
// Frequency of sparks is a percentage mapped to global var "animateSpeed"
// "animateSpeed" var contrained from 1 - 255 (0.4% - 100%)
// "fade" parameter specifies dropoff (next frame brightness = current frame brightness * (x/256)
// fade = 256 means no dropoff, pixels are on or off
// NOTE: this animation doesnt clear the previous buffer because the fade/dropoff is a function of the previous LED state
//***********************************************************************************
void Spark(CRGB targetStrip[], uint16_t animationFrame,uint8_t fade){
	uint8_t stripLength = sizeof(led_strip)/sizeof(CRGB);
	uint16_t rand = random16();
	
	for(int i=0;i<stripLength;i++)
		{		
			targetStrip[i].nscale8(fade);
		}
	

	if(rand < (MAX_INT_VALUE / (256 - (constrain(animateSpeed,1,255)))))
	{
		targetStrip[rand % stripLength].setHSV(0,0,255);
	}
}


//#######################################################################################################
//##                                      AGGREGATE ANIMATIONS                                         ##
//#######################################################################################################
void TripleBounce(CRGB strip[], uint16_t frame)		//3 chaser animations offset by 120 degrees each
{
	FastLED.clear();		//Clear previous buffer
	Bounce(strip,frame,0);
	Bounce(strip,frame+(MAX_INT_VALUE/3),100);
	Bounce(strip,frame+(MAX_INT_VALUE/3)*2,150);
}

void DoubleChaser(CRGB strip[], uint16_t frame)		//2 chaser animations offset 180 degrees
{
	FastLED.clear();		//Clear previous buffer
	frame = frame * 2;
	Ring(strip, frame, 0);
	Ring(strip, frame + (MAX_INT_VALUE / 2), 150);
}

void RingPair(CRGB strip[], uint16_t frame)			//2 rings animations at inverse phases
{
	FastLED.clear();		//Clear previous buffer
	Ring(strip, frame, 30);
	Ring(strip, MAX_INT_VALUE - frame, 150);
}


void RainbowSpark(CRGB targetStrip[], uint16_t animationFrame,uint8_t fade){    //Color spark where hue is function of frame
	Spark(targetStrip,animationFrame,fade,animationFrame/255);
	delay(20);
}

void Spin(CRGB targetStrip[], uint16_t animationFrame) {
	FastLED.clear();		//Clear previous buffer
	Ring(led_strip, animationFrame, 0);
}

void SpinStop(CRGB targetStrip[], uint16_t animationFrame) {
	FastLED.clear();		//Clear previous buffer

	if ((millis() - animationStartTime) > 10000) {
		fill_solid( &(led_strip[0]), NUM_LEDS, animateRGB);
	} else {
		Ring(led_strip, animationFrame, 0);
	}
}

void led_animation_loop() {
	//Each animation adjusts the "targetStrip" specified in its parameter.
	//Animations are a function of the current animation frame "frame"
	//Once you've applied the animation to the current strip/frame, Its up to the main loop to send the data to the strip(s)
	
	switch(animation) {
		case 0:
			FastLED.clear(true);
			break;
		case 1:
			fill_solid( &(led_strip[0]), NUM_LEDS, animateRGB);
			break;
		case 2:
			SpinStop(led_strip, frame);
			break;
		case 3:
			RingPair(led_strip, frame);	
			// Spin(led_strip, frame);
			// TripleBounce(led_strip,frame);
			break;
		case 4:
			WaveInt(led_strip,frame,180);
			break;
		case 5:  
			Wave(led_strip,frame,180);
			break;
		case 6:  //Blue spark (Slow)
			Spark(led_strip,frame,255,188);		//Overloaded version of "Spark" with Hue value, 255 for fade is the slowest fade possible. 256 is on/off
			delay(2);				//Slow things down a bit more for Slow Spark
			break;
		case 7:	//Blue spark (fast)
			Spark(led_strip,frame,246,188);		//Overloaded version of "Spark" with Hue value, 246 fade is faster which makes for a sharper dropoff
			break;
		case 8:  //White spark (Slow)
			Spark(led_strip,frame,255);			//"Spark" function without hue make a white spark, 255 for fade is the slowest fade possible.
			delay(2);				//Slow things down a bit more for Slow Spark
			break;
		case 9:	//White spark (fast)			//"Spark" function without hue make a white spark, 246 fade is faster which makes for a sharper dropoff
			Spark(led_strip,frame,245);
			break;
		case 10:
			RainbowSpark(led_strip,frame,240);		//240 for dropoff is a pretty sharp fade, good for this animation
			break;
		case 11:
			DoubleChaser(led_strip,frame);
			break;
		default:
			delay(100);   				//Animation OFF
	}
	
	FastLED.show();					//All animations are applied!..send the results to the strip(s)
	frame += animateSpeed;
}

