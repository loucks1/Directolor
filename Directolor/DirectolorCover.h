#include "esphome.h"
#include <433mhz.h>

Directolor directolor(22,21);

struct Directolor_Cover {
  bool supports_tilt;
  int time_for_full_movement;
  int remote;
  int blind;
};

const int time_for_full_tilt = 5;

class Cover433mhz : public Component, public Cover {
  public:

  void setup() override {
    setup433mhz();
  }

  void loop() override {
    directolor.processLoop();  //really just want a singleton of this - otherwise, it could go in the DirectolorCover
    loop433mhz();
  }

  CoverTraits get_traits() override {
    auto traits = CoverTraits();
    traits.set_is_assumed_state(true);
    traits.set_supports_position(false);
    traits.set_supports_tilt(false);
    return traits;
  }

  void control(const CoverCall &call) override {
    if (call.get_position().has_value()) {
      {
        float pos = *call.get_position();
        if (pos == 0)
          send433mhzCommand("close");
        if (pos == 1)
          send433mhzCommand("open");
      }
    }
  }
};

class DirectolorCover : public Component, public Cover {
 private:
  Directolor_Cover cover_settings;
  unsigned long millis_at_stop = 0;

 public:

  void setup() override {
    // This will be called by App.setup()
  }

  void loop() override {
   //directolor.processLoop();
   if (millis_at_stop != 0 && millis_at_stop < millis())
   {
     millis_at_stop = 0;
     directolor.sendCode(this->cover_settings.remote, this->cover_settings.blind, directolor_stop);
     ESP_LOGD("Directolor", "Issuing stop command for %d-%d", this->cover_settings.remote, this->cover_settings.blind);
   }
  }

  void setValues(Directolor_Cover cover_settings) {
     this->cover_settings = cover_settings;
}
  CoverTraits get_traits() override {
    auto traits = CoverTraits();
    traits.set_is_assumed_state(true);
    traits.set_supports_position(this->cover_settings.time_for_full_movement != 0);
    traits.set_supports_tilt(this->cover_settings.supports_tilt);
    return traits;
  }
  void control(const CoverCall &call) override {
    // This will be called every time the user requests a state change.
    if (call.get_position().has_value()) {
      float pos = *call.get_position();
      // Write pos (range 0-1) to cover
      // ...
      if (pos == 0)
	 directolor.sendCode(this->cover_settings.remote, this->cover_settings.blind, directolor_close);
      else if (pos == 1)
         directolor.sendCode(this->cover_settings.remote, this->cover_settings.blind,directolor_open);
      else {
	if (this->position == pos)
	 {
           ESP_LOGD("Directolor", "current position == requested position");
 	   return;
	 }
        if (this->position > pos)
	 directolor.sendCode(this->cover_settings.remote, this->cover_settings.blind, directolor_close);
        else
         directolor.sendCode(this->cover_settings.remote,this->cover_settings.blind,directolor_open);
	ESP_LOGD("Directolor", "current position %.2f requested position %.2f total seconds for movement %d", this->position, pos, this->cover_settings.time_for_full_movement);
	int desiredDelay = this->cover_settings.time_for_full_movement * 1000 * abs(this->position-pos);  //gives us the milliseconds of delay. 
    	ESP_LOGD("Directolor", "desiredDelay: %d", desiredDelay);
	millis_at_stop = millis() + desiredDelay;
      }
      // Publish new state
      this->position = pos;
      if (this->cover_settings.supports_tilt)
        this->tilt = 0;
      this->publish_state();
    }
    if (call.get_stop()) {
      // User requested cover stop
     directolor.sendCode(this->cover_settings.remote, this->cover_settings.blind, directolor_stop);
    }
    if (call.get_tilt()) {
      float tilt = *call.get_tilt();
      if (tilt == 0)
      {
	 directolor.sendCode(this->cover_settings.remote, this->cover_settings.blind, directolor_tiltClose);
      }
      else if (tilt == 1)
      {
	 directolor.sendCode(this->cover_settings.remote, this->cover_settings.blind, directolor_tiltOpen);
      }
      else
      {
	if (this->tilt == tilt)
	{
	  ESP_LOGD("Directolor", "current tilt = requested tilt");
	  return;
	}
	if (this->tilt > tilt)
	 directolor.sendCode(this->cover_settings.remote, this->cover_settings.blind, directolor_tiltClose);
	else
	 directolor.sendCode(this->cover_settings.remote, this->cover_settings.blind, directolor_tiltOpen);
	ESP_LOGD("Directolor", "current tilt %.2f requested tilt %.2f total seconds for tilt %d", this->tilt, tilt, time_for_full_tilt);
	int desiredDelay = time_for_full_tilt * 1000 * abs(this->tilt-tilt);  //gives us the milliseconds of delay. 
    	ESP_LOGD("Directolor", "desiredDelay: %d", desiredDelay);
	millis_at_stop = millis() + desiredDelay;
      }
      this->tilt = tilt;
      this->position = 0;
      this->publish_state();
    }
  }
};
