#ifndef _MENU_H
#define _MENU_H

#include "heltec.h"
#include "common/Timeout.h"
#include "common/WinchMessages.h"
#include "common/LED.h"

static void paintWinchStatus(WinchStatusUncompressed* winchStatus, LinkQualityClass* linkQuality) {
    static char textBuf[25];
    Heltec.display->setFont(ArialMT_Plain_16);

    Heltec.display->setTextAlignment(TEXT_ALIGN_LEFT);
    sprintf(textBuf, "%d / %dKg", (int)winchStatus->currentForce, (int)winchStatus->launchForce);
    Heltec.display->drawString(0, 31, textBuf);
    sprintf(textBuf, "%dm", (int)winchStatus->lineLength);
    Heltec.display->drawString(0, 47, textBuf);
    

    Heltec.display->setTextAlignment(TEXT_ALIGN_RIGHT);
    sprintf(textBuf, "%dm/s", (int)winchStatus->lineSpeed);
    Heltec.display->drawString(127, 31, textBuf);
    sprintf(textBuf, "%d%%", (int)(winchStatus->remainingCapacity*100));
    Heltec.display->drawString(127, 47, textBuf);
    //sprintf(textBuf, "%ddBm %d%%", (int)(winchStatus->signalStrength), (int)(100*linkQuality->getSuccessRate()));
    //Heltec.display->drawString(127, 47, textBuf);
}

//Forward declarations of states
class MenuStateClass;
class MenuCtxClass {
    public:
        struct {
            MenuStateClass* LOCKED;
            MenuStateClass* ROOT;
            MenuStateClass* LAUNCH;
            MenuStateClass* REWIND;
            MenuStateClass* CONFIG;
        } states;
        MenuStateClass* options[3];
        ControllerPacketManager* controllerPacket;
        WinchStatusUncompressed* winchStatus;
        LinkQualityClass* linkQuality;
        LEDClass* led;
};

class MenuStateClass {
    private:
        char* name;
        Timeout* timeout = NULL;
        ControllerMode mode;
    public:
        MenuStateClass(char* name, ControllerMode mode, uint32_t timeoutInterval = 0) {
            this->name = name;
            this->mode = mode;
            if (timeoutInterval>0) {
                timeout = new Timeout(0, timeoutInterval);
            }
        }
        ControllerMode getMode() {
            return mode;
        }
        virtual char* getName(MenuCtxClass* ctx) {
            return name;
        }
        virtual void enter(MenuCtxClass* ctx) {}
        virtual void exit(MenuCtxClass* ctx) {}
        virtual MenuStateClass* singleClick(MenuCtxClass* ctx) {
            return this;
        }
        virtual MenuStateClass* longPress(MenuCtxClass* ctx) {
            return this;
        }
        virtual MenuStateClass* longPressEnd(MenuCtxClass* ctx) {
            return this;
        }
        virtual MenuStateClass* longRelease(MenuCtxClass* ctx) {
            return this;
        }
        virtual MenuStateClass* multiClick(MenuCtxClass* ctx) {
            return ctx->states.LOCKED;
        }
        virtual MenuStateClass* onTimeout(MenuCtxClass* ctx) {
            return this;
        }
        
        MenuStateClass* tick(MenuCtxClass* ctx, uint32_t now) {
            if (timeout!=NULL && timeout->isTimeout(now)) {
                return onTimeout(ctx);
            }
            return this;
        }
        void resetTimeout(MenuCtxClass* ctx, uint32_t now) {
            if (timeout!=NULL) {
                timeout->reset(now);
            }
        }
        
        virtual void paint(MenuCtxClass* ctx) {}

        
        
};

class LOCKED_MenuStateClass : public MenuStateClass {
    public:
        LOCKED_MenuStateClass() : MenuStateClass("LOCKED", ControllerMode::IDLE) {}
        MenuStateClass* multiClick(MenuCtxClass* ctx) {
            return ctx->states.ROOT;
        }

        void paint(MenuCtxClass* ctx) {
            Heltec.display->setTextAlignment(TEXT_ALIGN_CENTER);
            Heltec.display->setFont(ArialMT_Plain_16);
            Heltec.display->drawString(64, 15, getName(ctx));
            paintWinchStatus(ctx->winchStatus, ctx->linkQuality);
        }
};

class ROOT_MenuStateClass : public MenuStateClass {
    private:
        int index = 0;
    public:
        ROOT_MenuStateClass() : MenuStateClass("ROOT", ControllerMode::IDLE, 5000) {}
        void enter(MenuCtxClass* ctx) {
            index = 0;
        }
        MenuStateClass* singleClick(MenuCtxClass* ctx) {
            index++;
            if (index>=3)  {
                index = 0;
            }
            return this;
        }
        
        MenuStateClass* longPress(MenuCtxClass* ctx) {
            return ctx->options[index];
        }
        MenuStateClass* onTimeout(MenuCtxClass* ctx) {
            return ctx->states.LOCKED;
        }
        
        void paint(MenuCtxClass* ctx) {

            Heltec.display->setTextAlignment(TEXT_ALIGN_LEFT);
            Heltec.display->setFont(ArialMT_Plain_16);
            for (int i=0;i<3;i++) {
                Heltec.display->setTextAlignment(TEXT_ALIGN_CENTER);
                Heltec.display->setFont(ArialMT_Plain_16);
                if (i==index) {
                    Heltec.display->fillRect(0, 15 + i*16, 128, 16);
                    Heltec.display->setColor(BLACK);
                    Heltec.display->drawString(64, 15 + i*16, ctx->options[i]->getName(ctx));
                    Heltec.display->setColor(WHITE);
                } else {
                    Heltec.display->drawString(64, 15 + i*16, ctx->options[i]->getName(ctx));
                }
            }
        }
};

class LAUNCH_MenuStateClass : public MenuStateClass {
    public:
        LAUNCH_MenuStateClass() : MenuStateClass("LAUNCH", ControllerMode::LAUNCH) {}

        MenuStateClass* longPress(MenuCtxClass* ctx) {
            ctx->controllerPacket->setPulling(true);
            return this;
        }
        MenuStateClass* longPressEnd(MenuCtxClass* ctx) {
            ctx->controllerPacket->setPulling(false);
            return this;
        }
        void paint(MenuCtxClass* ctx) {
            Heltec.display->setTextAlignment(TEXT_ALIGN_CENTER);
            Heltec.display->setFont(ArialMT_Plain_16);
            
            if (ctx->controllerPacket->isPulling()) {
                Heltec.display->fillRect(0, 15, 128, 16);
                Heltec.display->setColor(BLACK);
                Heltec.display->drawString(64, 15, "LAUNCH");
                Heltec.display->setColor(WHITE);   
            } else {
                Heltec.display->drawString(64, 15, "LAUNCH");
            }

            paintWinchStatus(ctx->winchStatus, ctx->linkQuality);
        }

};
class REWIND_MenuStateClass : public MenuStateClass {
    
    public:
        REWIND_MenuStateClass() : MenuStateClass("REWIND", ControllerMode::REWIND) {}


        MenuStateClass* longPress(MenuCtxClass* ctx) {
            ctx->controllerPacket->setPulling(true);
            return this;
        }
        MenuStateClass* longPressEnd(MenuCtxClass* ctx) {
            ctx->controllerPacket->setPulling(false);
            return this;
        }
        void paint(MenuCtxClass* ctx) {
            Heltec.display->setTextAlignment(TEXT_ALIGN_CENTER);
            Heltec.display->setFont(ArialMT_Plain_16);
            if (ctx->controllerPacket->isPulling()) {
                Heltec.display->fillRect(0, 15, 128, 16);
                Heltec.display->setColor(BLACK);
                Heltec.display->drawString(64, 15, "REWIND");
                Heltec.display->setColor(WHITE);   
            } else {
                Heltec.display->drawString(64, 15, "REWIND");
            }
            paintWinchStatus(ctx->winchStatus, ctx->linkQuality);
        }

};
class CONFIG_MenuStateClass : public MenuStateClass {
    private:
        
        char lineBuffer[20];
    public:
        CONFIG_MenuStateClass() : MenuStateClass("SET FORCE", ControllerMode::IDLE) {}
        char* getName(MenuCtxClass* ctx) {
            sprintf(lineBuffer, "FORCE %dKg", ctx->controllerPacket->getForce());
            return lineBuffer;
        }

        MenuStateClass* singleClick(MenuCtxClass* ctx) {
            ctx->controllerPacket->incForceIdx();
            return this;
        }
        MenuStateClass* longPress(MenuCtxClass* ctx) {
            return ctx->states.ROOT;
        }
        void paint(MenuCtxClass* ctx) {
            Heltec.display->setTextAlignment(TEXT_ALIGN_CENTER);
            Heltec.display->setFont(ArialMT_Plain_16);
            Heltec.display->drawString(64, 15, "SET FORCE");
            sprintf(lineBuffer, "%dKg", ctx->controllerPacket->getForce());
            Heltec.display->drawString(64, 15+16, lineBuffer);
        }
};



class MenuClass {
    private:
        //instantiate a single instance of each state
        LOCKED_MenuStateClass LOCKED;
        ROOT_MenuStateClass ROOT;
        LAUNCH_MenuStateClass LAUNCH;
        REWIND_MenuStateClass REWIND;
        CONFIG_MenuStateClass CONFIG;

        //Context object to pass to state machine states
        MenuCtxClass ctx;

        //Pointer to the current state
        MenuStateClass* currentState = NULL;

        void handleStateTransition(MenuStateClass* newState) {
            if (currentState==NULL || newState!=currentState) {
                if (currentState!=NULL) {
                    currentState->exit(&ctx);
                }
                currentState = newState;
                currentState->enter(&ctx);
                //reset mode and pulling flag in controller packet whenever transitioning states
                ctx.controllerPacket->setMode(currentState->getMode());
                ctx.controllerPacket->setPulling(false);
            }
        }
    public:
        MenuClass (ControllerPacketManager* controllerPacket, WinchStatusUncompressed* winchStatus, LinkQualityClass* linkQuality, LEDClass* led) {
            //populate state registry
            ctx.states.LOCKED = &LOCKED;
            ctx.states.ROOT = &ROOT;
            ctx.states.LAUNCH = &LAUNCH;
            ctx.states.REWIND = &REWIND;
            ctx.states.CONFIG = &CONFIG;
            //populate menu options
            ctx.options[0] = ctx.states.LAUNCH;
            ctx.options[1] = ctx.states.REWIND;
            ctx.options[2] = ctx.states.CONFIG;
            ctx.controllerPacket = controllerPacket;
            ctx.winchStatus = winchStatus;
            ctx.linkQuality = linkQuality;
            ctx.led = led;
            //transition to initial (LOCKED) state
            handleStateTransition(ctx.states.LOCKED);
        }

        void singleClick(uint32_t now) {
            handleStateTransition(currentState->singleClick(&ctx));
            currentState->resetTimeout(&ctx, now);
        }
        void longPress(uint32_t now) {
            handleStateTransition(currentState->longPress(&ctx));
            currentState->resetTimeout(&ctx, now);
        }
        void multiClick(uint32_t now) {
            handleStateTransition(currentState->multiClick(&ctx));
            currentState->resetTimeout(&ctx, now);
        }
        void longPressEnd(uint32_t now) {
            handleStateTransition(currentState->longPressEnd(&ctx));
            currentState->resetTimeout(&ctx, now);
        }
        void longRelease(uint32_t now) {
            handleStateTransition(currentState->longRelease(&ctx));
            currentState->resetTimeout(&ctx, now);
        }
        void tick(uint32_t now) {
            handleStateTransition(currentState->tick(&ctx, now));
        }
        void paint() {
            currentState->paint(&ctx);
        }
};



#endif
 