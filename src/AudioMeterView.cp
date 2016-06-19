// AudioMeterView.cp

#include "AudioMeterView.h"

#include <media/Subscriber.h>
#include <media/AudioStream.h>

#include <interface/PopUpMenu.h>

static const short shades_g[] = {80, 88, 97, 106, 115, 124, 132, 141, 150, 159,
								168, 176, 185, 194, 203, 212, 185, 150, 106, 0};
static const short shades_r[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
								0, 0, 0, 0, 0, 106, 150, 185, 212, 247};
double AudioMeterView::fRange;
double AudioMeterView::fPeakTime;


AudioMeterView::AudioMeterView(BMessage *archive) : BView(archive)
{
	do_init(	archive->FindBool("stream"), 
				archive->FindInt16("speed"), 
				archive->FindInt16("old_speed"),
				archive->FindDouble("range"),			// added this one,
				archive->FindDouble("peak_time"));		// and this one.
}

AudioMeterView::AudioMeterView(BRect r)
: BView(r, "AudioMeterView", B_FOLLOW_NONE, B_WILL_DRAW)
{
	do_init(0, 1, 1, 20.0, 1.0);			// changed false to 0, added two more parameters
}

AudioMeterView::~AudioMeterView()
{
	stop();
	
	delete scribe;
	delete adc_stream;
	delete dac_stream;
	
	delete pop;
	delete meterBits;
//	buf_lock->Lock();
	
//	delete l_buf;
//	delete r_buf;

//	buf_lock->Unlock();
//	delete buf_lock;
}

void AudioMeterView::do_init(int8 stream, int8 speed, int8 old_speed, double range, double peak_time)
{
	done = false;
	ready = false;
	
	fRange = range;
	fPeakTime = peak_time;
	
	BRect r = Bounds();
	meterBits = new BBitmap(r, B_COLOR_8_BIT, true);
	meterBits->AddChild(meters = new MeterView);
	
	/*
		Menu:
			Pause
			ADC
			DAC
			-
			Update Speed: 1/25 - 1/2 - 1 - 2 - 3 - 4 - 5
			Meter Range: 90dB - 60dB - 30dB - 20dB - 10dB
			Decay Rate: fast, medium, slow
	*/
	speed_menu = new BPopUpMenu("Update Speed", false, false);
	range_menu = new BPopUpMenu("Meter Range", false, false);				// add this menu
	decay_menu = new BPopUpMenu("Decay Rate", false, false);				// and this one
	pop = new BPopUpMenu("Audio Meter Menu");
	i125 = new BMenuItem("Realtime", new BMessage('1/25'));
	i12 = new BMenuItem("Fast", new BMessage('1/2 '));
	i1 = new BMenuItem("Average", new BMessage('1   '));
	i2 = new BMenuItem("Slow", new BMessage('2   '));
	pause = new BMenuItem("Pause", new BMessage('pase'));		// changed to lower case, Be reserve
	adc = new BMenuItem("ADC", new BMessage('adc '));			// all uppercase only message codes
	dac = new BMenuItem("DAC", new BMessage('dac '));
	range90 = new BMenuItem("90 dB", new BMessage('90dB'));
	range60 = new BMenuItem("60 dB", new BMessage('60dB'));
	range30 = new BMenuItem("30 dB", new BMessage('30dB'));
	range20 = new BMenuItem("20 dB", new BMessage('20dB'));
	range10 = new BMenuItem("10 dB", new BMessage('10dB'));
	decay1 = new BMenuItem("fast", new BMessage('dyft'));
	decay2 = new BMenuItem("medium", new BMessage('dymm'));
	decay3 = new BMenuItem("slow", new BMessage('dysw'));
	
	speed_menu->AddItem(i125);
	speed_menu->AddItem(i12);
	speed_menu->AddItem(i1);
	speed_menu->AddItem(i2);
	
	range_menu->AddItem(range10);
	range_menu->AddItem(range20);
	range_menu->AddItem(range30);
	range_menu->AddItem(range60);
	range_menu->AddItem(range90);
	
	decay_menu->AddItem(decay1);
	decay_menu->AddItem(decay2);
	decay_menu->AddItem(decay3);
	
	pop->AddItem(pause);
	pop->AddItem(adc);
	pop->AddItem(dac);
	pop->AddSeparatorItem();
	pop->AddItem(speed_menu);
	pop->AddItem(range_menu);
	pop->AddItem(decay_menu);
	
	(speed_selection = i12)->SetMarked(true);
	(range_selection = range20)->SetMarked(true);
	(decay_selection = decay2)->SetMarked(true);
	
//	l_buf = new float[512];
//	r_buf = new float[512];
	
//	buf_lock = new Benaphore("Buffer Lock");
	
	BRect		f = Frame();
	BRect		dragFrame(1, f.bottom - 7, 8, f.bottom);
	
	AddChild(new BDragger(dragFrame, this, 0));
	
	SetViewColor (0, 0, 0);
	
	u_thread = spawn_thread((thread_entry)u_func, "update thread", B_DISPLAY_PRIORITY, this);		// changed to display priority
	
	this->stream = stream;
	this->speed = speed;
	this->old_speed = old_speed;
	
	adc_stream = new BADCStream();
	
//	adc_stream->SetSamplingRate(44100);		// just metering, don't change anything
//	adc_stream->BoostMic(false);
	
	dac_stream = new BDACStream();
	
//	dac_stream->SetSamplingRate(44100);
	
	scribe = new BSubscriber();
	
	if (stream == 0)
		scribe->Subscribe(adc_stream);
	else if (stream == 1)
		scribe->Subscribe(dac_stream);
	else if (stream == 2)
	{
		speed = 0;
		old_speed = 2;
		return;
	}
	
	l_high = 0.0;
	l_peak = 0.0;
	l_peak_time = 0.0;
	r_high = 0.0;
	r_peak = 0.0;
	r_peak_time = 0.0;
	levels[0] = -1.0;		// will force a redraw very soon
	levels[1] = 0.0;
	levels[2] = 0.0;
	levels[3] = 0.0;
	
	scribe->EnterStream(NULL, true, (void *)this, (enter_stream_hook)s_func, NULL, true);
	
	resume_thread(u_thread);
}

void AudioMeterView::stop()
{
	done = true;
	
	status_t	result;
	
	resume_thread(u_thread);
	
	wait_for_thread(u_thread, &result);
	
	scribe->ExitStream();
	scribe->Unsubscribe();
}

void AudioMeterView::MouseDown(BPoint where)
{
	ConvertToScreen(&where);
	
	BMenuItem *choice = pop->Go(where, true, false, false);

	if (choice) UpdateData(choice->Command());
}

void AudioMeterView::UpdateData(int32 code)
{
	switch (code)
	{
		case 'pase':
			// Pause the threads
	
			if (speed == 0)
				break;
			
//			buf_lock->Lock();
//			for (int32 i = 0; i < 512; i++)
//			{
//				l_buf[i] = 0;
//				r_buf[i] = 0;
//			}
//			buf_lock->Unlock();	
			
			scribe->ExitStream();
			scribe->Unsubscribe();
			
			//suspend_thread(u_thread);
			old_speed = speed;
			speed = 0;
			
			stream = 2;
			l_peak = 0.0;
			r_peak = 0.0;
			l_high = 0.0;
			r_high = 0.0;
			l_peak_time = 0.0;
			r_peak_time = 0.0;
			levels[0] = -1.0;		// will force a redraw very soon
			levels[1] = 0.0;
			levels[2] = 0.0;
			levels[3] = 0.0;
			break;
		case 'adc ':
			// Switch to adc
			
			if (speed == 0)
			{
				speed = old_speed;
				resume_thread(u_thread);
			}
			
//			buf_lock->Lock();
//			for (int32 i = 0; i < 512; i++)
//			{
//				l_buf[i] = 0;
//				r_buf[i] = 0;
//			}
//			buf_lock->Unlock();
		
			scribe->ExitStream();
			scribe->Unsubscribe();
			
			scribe->Subscribe(adc_stream);
		
			stream = 0;
			l_peak = 0.0;
			r_peak = 0.0;
			l_high = 0.0;
			r_high = 0.0;
			l_peak_time = 0.0;
			r_peak_time = 0.0;
			levels[0] = -1.0;		// will force a redraw very soon
			levels[1] = 0.0;
			levels[2] = 0.0;
			levels[3] = 0.0;
	
			scribe->EnterStream(NULL, false, (void *)this, (enter_stream_hook)s_func, NULL, true);
			break;
		case 'dac ':
			// Switch to dac
			
			if (speed == 0)
			{
				speed = old_speed;
				resume_thread(u_thread);
			}
			
//			buf_lock->Lock();
//			for (int32 i = 0; i < 512; i++)
//			{
//				l_buf[i] = 0;
//				r_buf[i] = 0;
//			}
//			buf_lock->Unlock();	
		
			scribe->ExitStream();
			scribe->Unsubscribe();
			
			scribe->Subscribe(dac_stream);
		
			stream = 1;
			l_peak = 0.0;
			r_peak = 0.0;
			l_high = 0.0;
			r_high = 0.0;
			l_peak_time = 0.0;
			r_peak_time = 0.0;
			levels[0] = -1.0;		// will force a redraw very soon
			levels[1] = 0.0;
			levels[2] = 0.0;
			levels[3] = 0.0;
	
			scribe->EnterStream(NULL, false, (void *)this, (enter_stream_hook)s_func, NULL, true);
			break;
		case '1/25':
			if (speed != 0)
				speed = 1;
			else
				old_speed = 1;
			speed_selection->SetMarked(false);
			(speed_selection = i125)->SetMarked(true);
			break;
		case '1/2 ':
			if (speed != 0)
				speed = 2;
			else
				old_speed = 2;
			speed_selection->SetMarked(false);
			(speed_selection = i12)->SetMarked(true);
			break;
		case '1   ':
			if (speed != 0)
				speed = 3;
			else
				old_speed = 3;
			speed_selection->SetMarked(false);
			(speed_selection = i1)->SetMarked(true);
			break;
		case '2   ':
			if (speed != 0)
				speed = 4;
			else
				old_speed = 4;
			speed_selection->SetMarked(false);
			(speed_selection = i2)->SetMarked(true);
			break;
		case '10dB':
			fRange = 10.0;
			range_selection->SetMarked(false);
			(range_selection = range10)->SetMarked(true);
			break;
		case '20dB':
			fRange = 20.0;
			range_selection->SetMarked(false);
			(range_selection = range20)->SetMarked(true);
			break;
		case '30dB':
			fRange = 30.0;
			range_selection->SetMarked(false);
			(range_selection = range30)->SetMarked(true);
			break;
		case '60dB':
			fRange = 60.0;
			range_selection->SetMarked(false);
			(range_selection = range60)->SetMarked(true);
			break;
		case '90dB':
			fRange = 90.0;
			range_selection->SetMarked(false);
			(range_selection = range90)->SetMarked(true);
			break;
		case 'dysw':
			fPeakTime = 5.0;
			decay_selection->SetMarked(false);
			(decay_selection = decay3)->SetMarked(true);
			break;
		case 'dymm':
			fPeakTime = 2.0;
			decay_selection->SetMarked(false);
			(decay_selection = decay2)->SetMarked(true);
			break;
		case 'dyft':
			fPeakTime = 0.5;
			decay_selection->SetMarked(false);
			(decay_selection = decay1)->SetMarked(true);
			break;
		default:
			break;
	}
}

status_t AudioMeterView::Archive(BMessage *data, bool deep) const
{
	BView::Archive(data, deep);
	
	data->AddString("class", "AudioMeterView");
	data->AddString("add_on", "application/x-vnd.MattConnolly-AM");
	data->AddBool("stream", stream);
	data->AddInt16("speed", speed);
	data->AddInt16("old_speed", old_speed);
	data->AddDouble("range", fRange);
	data->AddDouble("peak_time", fPeakTime);
	
	return B_OK;
}

AudioMeterView *AudioMeterView::Instantiate(BMessage *data)
{
	if (validate_instantiation (data, "AudioMeterView"))
		return new AudioMeterView(data);
	else
		return NULL;
}

int32 AudioMeterView::u_func(void *data)
{
	AudioMeterView		*amv = (AudioMeterView *)data;
		
	amv->Update();

	return 0;
}

void AudioMeterView::Update(void)
{
	float			x;
	
	int8			l_cnt = 0;
	int8			r_cnt = 0;
	
	while (!ready) {
	}
	
	int32 old_time = time - 1;
	bigtime_t snooze_time;
	const bigtime_t little_snooze = 4000;
	
	while (!done) {
		// work out our snooze time
		switch (speed) {
			case 0:
				suspend_thread(find_thread(NULL));
				break;
			case 1:
				snooze_time = 20000;		// changed to 20000, approx 50 fps
				break;
			case 2:
				snooze_time = 33333;		// changed to 33333, approx 30 fps
				break;
			case 3:
				snooze_time = 50000;		// changed to 50000, approx 20 fps
				break;
			case 4:
				snooze_time = 100000;		// changed to 100000, approx 10 fps
				break;
		}
		// wait to go
		while (old_time == time && snooze_time > 0) {	// if no samples have been processed, have a little snooze
			snooze_time -= little_snooze;
			snooze(little_snooze);
		}
		old_time = time;
		if (snooze_time <= 0)
			snooze_time = little_snooze;	// snooze for at least this amount when done
		
//		l_cnt = 0;
//		r_cnt = 0;
//		buf_lock->Lock();
		
//		fft_real_to_hermitian(l_buf, 512);
//		fft_real_to_hermitian(r_buf, 512);
		
//		for (int32 i = 0; i < 512; i++)
//			l_high = l_buf[i] > l_high ? l_buf[i] : l_high;
			
//		for (int32 i = 0; i < 512; i++)
//			r_high = r_buf[i] > r_high ? r_buf[i] : r_high;
			
//		buf_lock->Unlock();
		
		// calculate the bar graph position, and peak positions
		double min;
		double high;
		float lhx;
		float lpx;
		float rhx;
		float rpx;
		
		if (l_high > 0) {
			lhx = fRange + 20 * log10(l_high);
			lhx /= fRange;
		} else {
			lhx = 0;
		}
		if (l_peak > 0) {
			lpx = fRange + 20 * log10(l_peak);
			lpx /= fRange;
		} else {
			lpx = 0;
		}
		if (r_high > 0) {
			rhx = fRange + 20 * log10(r_high);
			rhx /= fRange;
		} else {
			rhx = 0;
		}
		if (r_peak > 0) {
			rpx = fRange + 20 * log10(r_peak);
			rpx /= fRange;
		} else {
			rpx = 0;
		}
		
		// update window, if different
		if (lhx != levels[0] || lpx != levels[1] || rhx != levels[2] || rpx != levels[3]) {
			// update levels for next time
			levels[0] = lhx;
			levels[1] = lpx;
			levels[2] = rhx;
			levels[3] = rpx;
			
			meterBits->Lock();
			meters->DrawMeters(lhx, lpx, rhx, rpx);
			meterBits->Unlock();
			
			// lock the window
			Window()->Lock();
			DrawBitmapAsync(meterBits, Bounds());
			
//			for (int i = 0; i < 20; i++)
//			{
//			}
//		
//			// Draw Left Channel
//			for (int i = 0; i < l_cnt; i++)
//			{
//				SetHighColor(shades_r[i], shades_g[i], 0);
//				FillRect(BRect((i * 7) + 2, 2, (i * 7) + 7, 2 + PROGRESS_HEIGHT - 1));
//			}
//		
//			// Draw Right Channel
//			for (int i = 0; i < r_cnt; i++)
//			{
//				SetHighColor(shades_r[i], shades_g[i], 0);
//				FillRect(BRect((i * 7) + 2, PROGRESS_HEIGHT + 2, (i * 7) + 7, PROGRESS_HEIGHT + PROGRESS_HEIGHT - 1));
//			}
		
			// finish up
			Window()->Unlock();
			Flush();
		}
		
		// now we have a snooze
		snooze(snooze_time);
	}
}

// The position of the meters is calculated by the real time stream
// function. This way, regardless of what timing the display actually
// has, it will be fairly realistic. I am simply interested in peak
// values of the stream, so no fancy fft is required.

bool AudioMeterView::s_func(void *data, char *buf, size_t size, void *header)
{
	AudioMeterView	*amv = (AudioMeterView *)data;
	int32			count = size / 4;		// number of samples
	
	// update our current stream settings
	float sample_rate;
	if (amv->stream == 0)
		amv->adc_stream->SamplingRate(&sample_rate);
	else
		amv->dac_stream->SamplingRate(&sample_rate);
	double peak_samples = sample_rate * fPeakTime;
	double delta = -1.0 / peak_samples;
	double time_delta = 1.0 / sample_rate;
	double decay = pow(0.001, 1.0 / peak_samples);
	
	// localise certain class variables
	int16 *sample_ptr = (int16*) buf;
	int16 sample_left;
	int16 sample_right;
	double f_left;
	double f_right;
	double l_high = amv->l_high;
	double r_high = amv->r_high;
	double l_peak = amv->l_peak;
	double r_peak = amv->r_peak;
	double l_peak_time = amv->l_peak_time;
	double r_peak_time = amv->r_peak_time;
	
	for(int32 i = 0; i < count; i++)
	{
		// read samples
		sample_left = *sample_ptr++;
		sample_right = *sample_ptr++;
		// make positive floats
		if (sample_left < 0) f_left = sample_left / -32768.0;
		else f_left = sample_left / 32768.0;
		if (sample_right < 0) f_right = sample_right / -32768.0;
		else f_right = sample_right / 32768.0;
		// adjust peak indicators down a bit
		l_peak_time -= time_delta;
		if (l_peak_time <= 0.0) l_peak = -1.0;
		r_peak_time -= time_delta;
		if (r_peak_time <= 0.0) r_peak = -1.0;
		l_high *= decay;
		r_high *= decay;
		// compare current samples
		if (f_left > l_high) l_high = f_left;
		if (f_left > l_peak) {
			l_peak = f_left;
			l_peak_time = fPeakTime;
		}
		if (f_right > r_high) r_high = f_right;
		if (f_right > r_peak) {
			r_peak = f_right;
			r_peak_time = fPeakTime;
		}
	}
	// store these variables back in the class object for next time.
	amv->l_high = l_high;
	amv->r_high = r_high;
	amv->l_peak = l_peak;
	amv->r_peak = r_peak;
	amv->l_peak_time = l_peak_time;
	amv->r_peak_time = r_peak_time;
	amv->time += count;
	
	return true;
}

void AudioMeterView::Draw(BRect updateRect)
{
	// force a first redraw, and go!
	if (!ready) {
		meterBits->Lock();
		meters->DrawMeters(0,0,0,0);
		meterBits->Unlock();
		ready = true;
	}
	// draw the bitmap
	BRect r = Bounds();
	DrawBitmap(meterBits, updateRect, updateRect);
}


void MeterView::DrawMeters(float lhx, float lpx, float rhx, float rpx)
{
	// background
	BRect r(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
	SetHighColor(0, 0, 0);
	FillRect(r);
	
	// left channel
	r.Set(2, 2, 140, 17);
	SetHighColor(75, 75, 75);
	FillRect(r);
	if (lhx > 0.0) {
		if (lhx > 1.0) lhx = 1.0;
		r.right = 2 + lhx * (WINDOW_WIDTH - 4);
		SetHighColor(0, 192, 0);
		FillRect(r);
		if (lpx > lhx) {
			SetHighColor(255, 255, 0);
			lpx = 2 + lpx * (WINDOW_WIDTH - 4);
			StrokeLine(BPoint(lpx, 2), BPoint(lpx, 17));
		}
	}
	
	// right channel
	r.Set(2, 20, 140, 35);
	SetHighColor(75, 75, 75);
	FillRect(r);
	if (rpx < rhx) {
		rpx = rhx;
	}
	if (rhx > 0.0) {
		if (rhx > 1.0) rhx = 1.0;
		r.right = 2 + rhx * (WINDOW_WIDTH - 4);
		SetHighColor(0, 192, 0);
		FillRect(r);
		if (rpx > rhx) {
			SetHighColor(255, 255, 0);
			rpx = 2 + rpx * (WINDOW_WIDTH - 4);
			StrokeLine(BPoint(rpx, 20), BPoint(rpx, 35));
		}
	}
	
	// render it
	Sync();
}
