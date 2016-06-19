// AudioMeter.cp

#include <new.h>
#include "AudioMeter.h"
#include "AudioMeterView.h"

// code

class AMWindow : public BWindow
{
	AudioMeterView *amv;
	
	public:
	
	AMWindow (void)
	 : BWindow(BRect(45, 45, 45 + WINDOW_WIDTH - 1, 45 + WINDOW_HEIGHT - 1),
	 			"Audio Meter", B_TITLED_WINDOW, B_NOT_RESIZABLE | B_NOT_ZOOMABLE)
	{
		Lock();
		AddChild(amv = new AudioMeterView(BRect(0, 0, WINDOW_WIDTH - 1, WINDOW_HEIGHT - 1)));
		Unlock();
	}
	
	virtual bool QuitRequested(void)
	{
		amv->stop();
		be_app->PostMessage(B_QUIT_REQUESTED);
		Quit();
		return true;
	}
};

AudioMeterApp::AudioMeterApp(void) : BApplication("application/x-vnd.MattConnolly-AM")
{
	// create the window as a part of the app's constructor
	fWindow = new AMWindow();
}

AudioMeterApp::~AudioMeterApp(void)
{
}

void AudioMeterApp::ReadyToRun(void)
{
	fWindow->Show();					// only show the window now.
	BApplication::ReadyToRun();		// call the superclass
}

// don't need to do this one; the BApplication class will do everything
bool AudioMeterApp::QuitRequested(void)
{
	return fWindow->QuitRequested();
// 	return true;
}

void AudioMeterApp::MessageReceived(BMessage *message)
{
	BApplication::MessageReceived(message);		// MUST do this, or the BApplication can't do it's stuff
}

main(int argc, char **argv)
{
	AudioMeterApp		*app = new AudioMeterApp();
	app->Run();
	delete app;
	
	return 0;
}