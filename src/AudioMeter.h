// AudioMeter.h

#pragma once

#include <Application.h>
#include <Window.h>

// forward declarations
class AMWindow;

// class definitions

class AudioMeterApp : public BApplication
{
public:
					AudioMeterApp(void);
					~AudioMeterApp(void);
				
	virtual void	ReadyToRun(void);
	virtual bool	QuitRequested(void);
	
	virtual void	MessageReceived(BMessage *message);
	
private:
	
		AMWindow	*fWindow;		
};

