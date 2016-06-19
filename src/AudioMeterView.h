// AudioMeterView.h

#pragma once

#include <View.h>
#include <interface/Bitmap.h>

enum {
	WINDOW_WIDTH = 142,
	WINDOW_HEIGHT = 38,
	BAR_HEIGHT = 16,
	PROGRESS_WIDTH	= 142,
	PROGRESS_HEIGHT	= 16
};

#pragma export on

class MeterView;

class AudioMeterView : public BView
{
	public:
						AudioMeterView(BMessage *archive);
						AudioMeterView(BRect r);
						
						~AudioMeterView();
						
	void				do_init(int8 stream, int8 speed, int8 old_speed, double range, double peak_time);
	void				stop();
	
	virtual status_t	Archive(BMessage *data, bool deep) const;
	static AudioMeterView *Instantiate(BMessage *data);
	
	virtual void		Draw(BRect updateRect);
	
	virtual void		MouseDown(BPoint where);
	virtual void		UpdateData(int32 code);
	
	private:
	
	BPopUpMenu		*speed_menu;
	BPopUpMenu		*range_menu;
	BPopUpMenu		*decay_menu;
	BPopUpMenu		*pop;
	BMenuItem		*i125;
	BMenuItem		*i12;
	BMenuItem		*i1;
	BMenuItem		*i2;
	BMenuItem		*pause;
	BMenuItem		*adc;
	BMenuItem		*dac;
	BMenuItem		*range90;
	BMenuItem		*range60;
	BMenuItem		*range30;
	BMenuItem		*range20;
	BMenuItem		*range10;
	BMenuItem		*decay1;
	BMenuItem		*decay2;
	BMenuItem		*decay3;
	BMenuItem		*speed_selection;
	BMenuItem		*range_selection;
	BMenuItem		*decay_selection;
	
	BBitmap				*meterBits;
	MeterView			*meters;
	
	bool				ready;
	bool				done;
	thread_id			u_thread;
	
	int8				stream;
	int8				speed;
	int8				old_speed;
static double			fRange;		// range in dB of entire display
static double			fPeakTime;	// hold the peak indicator for this many seconds.
	
volatile double			l_peak, r_peak, l_high, r_high;
volatile double			l_peak_time, r_peak_time;
volatile int32			time;
	float				levels[4];
	
	BADCStream			*adc_stream;
	BDACStream			*dac_stream;
	
	BSubscriber			*scribe;
	
	static int32		u_func(void *data);
	void				Update(void);
	static bool 		s_func(void *data, char *buf, size_t size, void *header);
};

class MeterView : public BView
{
public:
	MeterView() : BView( BRect(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT), "Meters", B_FOLLOW_NONE, B_WILL_DRAW) {}
	
	void DrawMeters(float lhx, float lpx, float rhx, float rpx);
};

#pragma export reset
