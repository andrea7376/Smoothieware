#include "CurrentControl.h"
#include "libs/Kernel.h"
#include "libs/nuts_bolts.h"
#include "libs/utils.h"
#include "ConfigValue.h"
#include "libs/StreamOutput.h"

#include "Gcode.h"
#include "Config.h"
#include "checksumm.h"
#include "DigipotBase.h"

// add new digipot chips here
#include "mcp4451.h"
#include "ad5206.h"

#include <string>
using namespace std;

#define alpha_current_checksum                  CHECKSUM("alpha_current")
#define beta_current_checksum                   CHECKSUM("beta_current")
#define gamma_current_checksum                  CHECKSUM("gamma_current")
#define delta_current_checksum                  CHECKSUM("delta_current")
#define epsilon_current_checksum                CHECKSUM("epsilon_current")
#define zeta_current_checksum                   CHECKSUM("zeta_current")
#define eta_current_checksum                    CHECKSUM("eta_current")
#define theta_current_checksum                  CHECKSUM("theta_current")
#define currentcontrol_module_enable_checksum   CHECKSUM("currentcontrol_module_enable")
#define digipotchip_checksum                    CHECKSUM("digipotchip")
#define digipot_max_current                     CHECKSUM("digipot_max_current")
#define digipot_factor                          CHECKSUM("digipot_factor")

#define mcp4451_checksum                        CHECKSUM("mcp4451")
#define ad5206_checksum                         CHECKSUM("ad5206")

CurrentControl::CurrentControl()
{
    digipot = NULL;
}

void CurrentControl::on_module_loaded()
{
    if( !THEKERNEL->config->value( currentcontrol_module_enable_checksum )->by_default(false)->as_bool() ) {
        // as this module is not needed free up the resource
        delete this;
        return;
    }

    // allocate digipot, if already allocated delete it first
    delete digipot;

    // see which chip to use
    int chip_checksum = get_checksum(THEKERNEL->config->value(digipotchip_checksum)->by_default("mcp4451")->as_string());
    if(chip_checksum == mcp4451_checksum) {
        digipot = new MCP4451();
    } else if(chip_checksum == ad5206_checksum) {
        digipot = new AD5206();
    } else { // need a default so use smoothie
        digipot = new MCP4451();
    }

    digipot->set_max_current( THEKERNEL->config->value(digipot_max_current )->by_default(2.0f)->as_number());
    digipot->set_factor( THEKERNEL->config->value(digipot_factor )->by_default(113.33f)->as_number());

    // Get configuration
    this->digipot->set_current(0, THEKERNEL->config->value(alpha_current_checksum  )->by_default(0.8f)->as_number());
    this->digipot->set_current(1, THEKERNEL->config->value(beta_current_checksum   )->by_default(0.8f)->as_number());
    this->digipot->set_current(2, THEKERNEL->config->value(gamma_current_checksum  )->by_default(0.8f)->as_number());
    this->digipot->set_current(3, THEKERNEL->config->value(delta_current_checksum  )->by_default(0.8f)->as_number());
    this->digipot->set_current(4, THEKERNEL->config->value(epsilon_current_checksum)->by_default(-1)->as_number());
    this->digipot->set_current(5, THEKERNEL->config->value(zeta_current_checksum   )->by_default(-1)->as_number());
    this->digipot->set_current(6, THEKERNEL->config->value(eta_current_checksum    )->by_default(-1)->as_number());
    this->digipot->set_current(7, THEKERNEL->config->value(theta_current_checksum  )->by_default(-1)->as_number());


    this->register_for_event(ON_GCODE_RECEIVED);
}


void CurrentControl::on_gcode_received(void *argument)
{
    Gcode *gcode = static_cast<Gcode*>(argument);
    if (gcode->has_m) {
        if (gcode->m == 907) {
            if(gcode->has_letter('E')) { // thi smeans it was the old setting, E is no longer allowed
                char alpha[] = { 'X', 'Y', 'Z', 'E', 'A', 'B', 'C' };
                gcode->stream->printf("WARNING: Using E is deprecated, use A and B for channels 3 and 4\n");
                // this is the old format where E => A, A => B, B => C, C => D
                for (int i = 0; i < 7; i++) {
                    if (gcode->has_letter(alpha[i])) {
                        this->digipot->set_current(i, gcode->get_value(alpha[i]));
                    }
                }

            }else{
                // new format where XYZ are the first 3 channels and ABCD are the next channels
                for (int i = 0; i < 7; i++) {
                    char axis= i < 3 ? 'X'+i : 'A'+i-3;
                    if (gcode->has_letter(axis)) {
                        float c = gcode->get_value(axis);
                        this->digipot->set_current(i, c);
                    }
                }
            }

        } else if(gcode->m == 500 || gcode->m == 503) {
            float currents[7];
            bool has_setting= false;
            for (int i = 0; i < 7; i++) {
                currents[i]= this->digipot->get_current(i);
                if(currents[i] >= 0) has_setting= true;
            }
            if(!has_setting) return; // don't ouput anything if none are set using this current control

            gcode->stream->printf(";Digipot Motor currents:\nM907 ");
            for (int i = 0; i < 7; i++) {
                char axis= i < 3 ? 'X'+i : 'A'+i-3;
                if(currents[i] >= 0)
                    gcode->stream->printf("%c%1.5f ", axis, currents[i]);
            }
            gcode->stream->printf("\n");
        }
    }
}
