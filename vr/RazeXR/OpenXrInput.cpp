#include "VrInput.h"

void TBXR_Log(const char *fmt, ...);	// TBXR_PC.cpp, writes razexr_vr.log
bool VR_IndexBindings();				// hw_vrmodes.cpp
static XrPath gIndexProfilePath = XR_NULL_PATH;

extern ovrApp gAppState;

XrResult CheckXrResult(XrResult res, const char* originator) {
    if (XR_FAILED(res)) {
        ALOGE("error: %s", originator);
    }
    return res;
}

#define CHECK_XRCMD(cmd) CheckXrResult(cmd, #cmd);

#define SIDE_LEFT 0
#define SIDE_RIGHT 1
#define SIDE_COUNT 2


XrActionSet actionSet = nullptr;
XrAction grabAction;
XrAction poseAction;
XrAction vibrateAction;
XrAction quitAction;
XrAction touchpadAction;
XrAction AXAction;
XrAction homeAction;
XrAction BYAction;
XrAction backAction;
XrAction sideAction;
XrAction triggerAction;
XrAction joystickAction;
XrAction batteryAction;
XrAction AXTouchAction;
XrAction BYTouchAction;
XrAction thumbstickTouchAction;
XrAction TriggerTouchAction;
XrAction ThumbrestTouchAction;
XrAction GripAction;
XrAction AAction;
XrAction BAction;
XrAction XAction;
XrAction YAction;
XrAction ATouchAction;
XrAction BTouchAction;
XrAction XTouchAction;
XrAction YTouchAction;
XrAction aimAction;

XrSpace aimSpace[SIDE_COUNT];
XrPath handSubactionPath[SIDE_COUNT];
XrSpace handSpace[SIDE_COUNT];



XrActionSuggestedBinding ActionSuggestedBinding(XrAction action, XrPath path) {
    XrActionSuggestedBinding asb;
    asb.action = action;
    asb.binding = path;
    return asb;
}

XrActionStateBoolean GetActionStateBoolean(XrAction action, int hand) {
    XrActionStateGetInfo getInfo = {};
    getInfo.type = XR_TYPE_ACTION_STATE_GET_INFO;
    getInfo.action = action;
    getInfo.next = NULL;
    if (hand >= 0)
        getInfo.subactionPath = handSubactionPath[hand];

    XrActionStateBoolean state = {};
    state.type = XR_TYPE_ACTION_STATE_BOOLEAN;
    state.next = NULL;
    CHECK_XRCMD(xrGetActionStateBoolean(gAppState.Session, &getInfo, &state));
    return state;
}

XrActionStateFloat GetActionStateFloat(XrAction action, int hand) {
    XrActionStateGetInfo getInfo = {};
    getInfo.type = XR_TYPE_ACTION_STATE_GET_INFO;
    getInfo.action = action;
    getInfo.next = NULL;
    if (hand >= 0)
        getInfo.subactionPath = handSubactionPath[hand];

    XrActionStateFloat state = {};
    state.type = XR_TYPE_ACTION_STATE_FLOAT;
    state.next = NULL;
    CHECK_XRCMD(xrGetActionStateFloat(gAppState.Session, &getInfo, &state));
    return state;
}

XrActionStateVector2f GetActionStateVector2(XrAction action, int hand) {
    XrActionStateGetInfo getInfo = {};
    getInfo.type = XR_TYPE_ACTION_STATE_GET_INFO;
    getInfo.action = action;
    getInfo.next = NULL;
    if (hand >= 0)
        getInfo.subactionPath = handSubactionPath[hand];

    XrActionStateVector2f state = {};
    state.type = XR_TYPE_ACTION_STATE_VECTOR2F;
    state.next = NULL;
    CHECK_XRCMD(xrGetActionStateVector2f(gAppState.Session, &getInfo, &state));
    return state;
}

void CreateAction(
        XrActionSet actionSet,
        XrActionType type,
        const char* actionName,
        const char* localizedName,
        int countSubactionPaths,
        XrPath* subactionPaths,
        XrAction* action) {
    ALOGV("CreateAction %s, %", actionName, countSubactionPaths);

    XrActionCreateInfo aci = {};
    aci.type = XR_TYPE_ACTION_CREATE_INFO;
    aci.next = NULL;
    aci.actionType = type;
    if (countSubactionPaths > 0) {
        aci.countSubactionPaths = countSubactionPaths;
        aci.subactionPaths = subactionPaths;
    }
    strcpy(aci.actionName, actionName);
    strcpy(aci.localizedActionName, localizedName ? localizedName : actionName);
    *action = XR_NULL_HANDLE;
    OXR(xrCreateAction(actionSet, &aci, action));
}

void TBXR_InitActions( void )
{
    // Create an action set.
    {
        XrActionSetCreateInfo actionSetInfo = {};
        actionSetInfo.type = XR_TYPE_ACTION_SET_CREATE_INFO;
        strcpy(actionSetInfo.actionSetName, "gameplay");
        strcpy(actionSetInfo.localizedActionSetName, "Gameplay");
        actionSetInfo.priority = 0;
        actionSetInfo.next = NULL;
        CHECK_XRCMD(xrCreateActionSet(gAppState.Instance, &actionSetInfo, &actionSet));
    }

    // Get the XrPath for the left and right hands - we will use them as subaction paths.
    CHECK_XRCMD(xrStringToPath(gAppState.Instance, "/user/hand/left", &handSubactionPath[SIDE_LEFT]));
    CHECK_XRCMD(xrStringToPath(gAppState.Instance, "/user/hand/right", &handSubactionPath[SIDE_RIGHT]));

    // Create actions.
    {
        // Create an input action for grabbing objects with the left and right hands.
        CreateAction(actionSet, XR_ACTION_TYPE_POSE_INPUT, "grab_object", "Grab Object", SIDE_COUNT, handSubactionPath, &grabAction);

        // Create an input action getting the left and right hand poses.
        CreateAction(actionSet, XR_ACTION_TYPE_POSE_INPUT, "hand_pose", "Hand Pose", SIDE_COUNT, handSubactionPath, &poseAction);
        CreateAction(actionSet, XR_ACTION_TYPE_POSE_INPUT, "aim_pose", "Aim Pose", SIDE_COUNT, handSubactionPath, &aimAction);

        // Create output actions for vibrating the left and right controller.
        CreateAction(actionSet, XR_ACTION_TYPE_VIBRATION_OUTPUT, "vibrate_hand", "Vibrate Hand", SIDE_COUNT, handSubactionPath, &vibrateAction);

        //All remaining actions (not necessarily supported by all controllers)
        /*
            PC branch: the second name of each action is what SteamVR's
            controller binding screen shows. Theirs were the button names
            ("Akey", "Touchpad" for the stick click, "Backkey" for the
            menu), and a player rebinding an Index asked for what each one
            does. They now name the default game action. Only that name
            changed: the first one is the key bindings are saved under.
            OpenXR refuses two actions with the same localized name, so each
            stays unique.
        */
        CreateAction(actionSet, XR_ACTION_TYPE_BOOLEAN_INPUT, "quit_session", "Quit Session", SIDE_COUNT, handSubactionPath, &quitAction);
        CreateAction(actionSet, XR_ACTION_TYPE_BOOLEAN_INPUT, "touchpad", "Stick click - weapon wheel, alt weapon", SIDE_COUNT, handSubactionPath, &touchpadAction);
        CreateAction(actionSet, XR_ACTION_TYPE_BOOLEAN_INPUT, "axkey", "AXkey", SIDE_COUNT, handSubactionPath, &AXAction);
        CreateAction(actionSet, XR_ACTION_TYPE_BOOLEAN_INPUT, "homekey", "Homekey", SIDE_COUNT, handSubactionPath, &homeAction);
        CreateAction(actionSet, XR_ACTION_TYPE_BOOLEAN_INPUT, "bykey", "BYkey", SIDE_COUNT, handSubactionPath, &BYAction);
        CreateAction(actionSet, XR_ACTION_TYPE_BOOLEAN_INPUT, "backkey", "Menu", SIDE_COUNT, handSubactionPath, &backAction);
        CreateAction(actionSet, XR_ACTION_TYPE_BOOLEAN_INPUT, "sidekey", "Grip click", SIDE_COUNT, handSubactionPath, &sideAction);
        CreateAction(actionSet, XR_ACTION_TYPE_FLOAT_INPUT, "trigger", "Trigger - fire, alt fire", SIDE_COUNT, handSubactionPath, &triggerAction);
        CreateAction(actionSet, XR_ACTION_TYPE_VECTOR2F_INPUT, "joystick", "Stick - move, turn, weapon up and down", SIDE_COUNT, handSubactionPath, &joystickAction);
        CreateAction(actionSet, XR_ACTION_TYPE_FLOAT_INPUT, "battery", "battery", SIDE_COUNT, handSubactionPath, &batteryAction);
        CreateAction(actionSet, XR_ACTION_TYPE_BOOLEAN_INPUT, "axtouch", "AXtouch", SIDE_COUNT, handSubactionPath, &AXTouchAction);
        CreateAction(actionSet, XR_ACTION_TYPE_BOOLEAN_INPUT, "bytouch", "BYtouch", SIDE_COUNT, handSubactionPath, &BYTouchAction);
        CreateAction(actionSet, XR_ACTION_TYPE_BOOLEAN_INPUT, "rockertouch", "Rockertouch", SIDE_COUNT, handSubactionPath, &thumbstickTouchAction);
        CreateAction(actionSet, XR_ACTION_TYPE_BOOLEAN_INPUT, "triggertouch", "Triggertouch", SIDE_COUNT, handSubactionPath, &TriggerTouchAction);
        CreateAction(actionSet, XR_ACTION_TYPE_BOOLEAN_INPUT, "thumbresttouch", "Thumbrest - quick kick", SIDE_COUNT, handSubactionPath, &ThumbrestTouchAction);
        CreateAction(actionSet, XR_ACTION_TYPE_FLOAT_INPUT, "gripvalue", "Grip - second set of controls", SIDE_COUNT, handSubactionPath, &GripAction);
        CreateAction(actionSet, XR_ACTION_TYPE_BOOLEAN_INPUT, "akey", "A - jump", SIDE_COUNT, handSubactionPath, &AAction);
        CreateAction(actionSet, XR_ACTION_TYPE_BOOLEAN_INPUT, "bkey", "B - open, use", SIDE_COUNT, handSubactionPath, &BAction);
        CreateAction(actionSet, XR_ACTION_TYPE_BOOLEAN_INPUT, "xkey", "X - crouch", SIDE_COUNT, handSubactionPath, &XAction);
        CreateAction(actionSet, XR_ACTION_TYPE_BOOLEAN_INPUT, "ykey", "Y - automap", SIDE_COUNT, handSubactionPath, &YAction);
        CreateAction(actionSet, XR_ACTION_TYPE_BOOLEAN_INPUT, "atouch", "Atouch", SIDE_COUNT, handSubactionPath, &ATouchAction);
        CreateAction(actionSet, XR_ACTION_TYPE_BOOLEAN_INPUT, "btouch", "Btouch", SIDE_COUNT, handSubactionPath, &BTouchAction);
        CreateAction(actionSet, XR_ACTION_TYPE_BOOLEAN_INPUT, "xtouch", "Xtouch", SIDE_COUNT, handSubactionPath, &XTouchAction);
        CreateAction(actionSet, XR_ACTION_TYPE_BOOLEAN_INPUT, "ytouch", "Ytouch", SIDE_COUNT, handSubactionPath, &YTouchAction);
    }

    XrPath selectPath[SIDE_COUNT];
    XrPath squeezeValuePath[SIDE_COUNT];
    XrPath squeezeClickPath[SIDE_COUNT];
    XrPath posePath[SIDE_COUNT];
    XrPath hapticPath[SIDE_COUNT];
    XrPath menuClickPath[SIDE_COUNT];
    XrPath systemPath[SIDE_COUNT];
    XrPath thumbrestPath[SIDE_COUNT];
    XrPath triggerTouchPath[SIDE_COUNT];
    XrPath triggerValuePath[SIDE_COUNT];
    XrPath thumbstickClickPath[SIDE_COUNT];
    XrPath thumbstickTouchPath[SIDE_COUNT];
    XrPath thumbstickPosPath[SIDE_COUNT];
    XrPath aimPath[SIDE_COUNT];

    XrPath touchpadPath[SIDE_COUNT];
    XrPath AXValuePath[SIDE_COUNT];
    XrPath homeClickPath[SIDE_COUNT];
    XrPath BYValuePath[SIDE_COUNT];
    XrPath backPath[SIDE_COUNT];
    XrPath sideClickPath[SIDE_COUNT];
    XrPath triggerPath[SIDE_COUNT];
    XrPath joystickPath[SIDE_COUNT];
    XrPath batteryPath[SIDE_COUNT];

    XrPath GripPath[SIDE_COUNT];
    XrPath AXTouchPath[SIDE_COUNT];
    XrPath BYTouchPath[SIDE_COUNT];
    XrPath RockerTouchPath[SIDE_COUNT];
    XrPath TriggerTouchPath[SIDE_COUNT];
    XrPath ThumbresetTouchPath[SIDE_COUNT];

    XrPath AValuePath[SIDE_COUNT];
    XrPath BValuePath[SIDE_COUNT];
    XrPath XValuePath[SIDE_COUNT];
    XrPath YValuePath[SIDE_COUNT];
    XrPath ATouchPath[SIDE_COUNT];
    XrPath BTouchPath[SIDE_COUNT];
    XrPath XTouchPath[SIDE_COUNT];
    XrPath YTouchPath[SIDE_COUNT];

    CHECK_XRCMD(xrStringToPath(gAppState.Instance, "/user/hand/left/input/select/click", &selectPath[SIDE_LEFT]));
    CHECK_XRCMD(xrStringToPath(gAppState.Instance, "/user/hand/right/input/select/click", &selectPath[SIDE_RIGHT]));
    CHECK_XRCMD(xrStringToPath(gAppState.Instance, "/user/hand/left/input/menu/click", &menuClickPath[SIDE_LEFT]));
    CHECK_XRCMD(xrStringToPath(gAppState.Instance, "/user/hand/right/input/menu/click", &menuClickPath[SIDE_RIGHT]));

    CHECK_XRCMD(xrStringToPath(gAppState.Instance, "/user/hand/left/input/squeeze/value", &squeezeValuePath[SIDE_LEFT]));
    CHECK_XRCMD(xrStringToPath(gAppState.Instance, "/user/hand/right/input/squeeze/value", &squeezeValuePath[SIDE_RIGHT]));
    CHECK_XRCMD(xrStringToPath(gAppState.Instance, "/user/hand/left/input/squeeze/click", &squeezeClickPath[SIDE_LEFT]));
    CHECK_XRCMD(xrStringToPath(gAppState.Instance, "/user/hand/right/input/squeeze/click", &squeezeClickPath[SIDE_RIGHT]));
    CHECK_XRCMD(xrStringToPath(gAppState.Instance, "/user/hand/left/input/grip/pose", &posePath[SIDE_LEFT]));
    CHECK_XRCMD(xrStringToPath(gAppState.Instance, "/user/hand/right/input/grip/pose", &posePath[SIDE_RIGHT]));
    CHECK_XRCMD(xrStringToPath(gAppState.Instance, "/user/hand/left/input/aim/pose", &aimPath[SIDE_LEFT]));
    CHECK_XRCMD(xrStringToPath(gAppState.Instance, "/user/hand/right/input/aim/pose", &aimPath[SIDE_RIGHT]));

    CHECK_XRCMD(xrStringToPath(gAppState.Instance, "/user/hand/left/output/haptic", &hapticPath[SIDE_LEFT]));
    CHECK_XRCMD(xrStringToPath(gAppState.Instance, "/user/hand/right/output/haptic", &hapticPath[SIDE_RIGHT]));

    CHECK_XRCMD(xrStringToPath(gAppState.Instance, "/user/hand/left/input/trigger/touch", &triggerTouchPath[SIDE_LEFT]));
    CHECK_XRCMD(xrStringToPath(gAppState.Instance, "/user/hand/right/input/trigger/touch", &triggerTouchPath[SIDE_RIGHT]));
    CHECK_XRCMD(xrStringToPath(gAppState.Instance, "/user/hand/left/input/trigger/value", &triggerValuePath[SIDE_LEFT]));
    CHECK_XRCMD(xrStringToPath(gAppState.Instance, "/user/hand/right/input/trigger/value", &triggerValuePath[SIDE_RIGHT]));

    CHECK_XRCMD(xrStringToPath(gAppState.Instance, "/user/hand/left/input/thumbstick/click", &thumbstickClickPath[SIDE_LEFT]));
    CHECK_XRCMD(xrStringToPath(gAppState.Instance, "/user/hand/right/input/thumbstick/click", &thumbstickClickPath[SIDE_RIGHT]));
    CHECK_XRCMD(xrStringToPath(gAppState.Instance, "/user/hand/left/input/thumbstick/touch", &thumbstickTouchPath[SIDE_LEFT]));
    CHECK_XRCMD(xrStringToPath(gAppState.Instance, "/user/hand/right/input/thumbstick/touch", &thumbstickTouchPath[SIDE_RIGHT]));
    CHECK_XRCMD(xrStringToPath(gAppState.Instance, "/user/hand/left/input/thumbstick", &thumbstickPosPath[SIDE_LEFT]));
    CHECK_XRCMD(xrStringToPath(gAppState.Instance, "/user/hand/right/input/thumbstick", &thumbstickPosPath[SIDE_RIGHT]));

    CHECK_XRCMD(xrStringToPath(gAppState.Instance, "/user/hand/left/input/system/click", &systemPath[SIDE_LEFT]));
    CHECK_XRCMD(xrStringToPath(gAppState.Instance, "/user/hand/right/input/system/click", &systemPath[SIDE_RIGHT]));
    CHECK_XRCMD(xrStringToPath(gAppState.Instance, "/user/hand/left/input/thumbrest/touch", &thumbrestPath[SIDE_LEFT]));
    CHECK_XRCMD(xrStringToPath(gAppState.Instance, "/user/hand/right/input/thumbrest/touch", &thumbrestPath[SIDE_RIGHT]));

    CHECK_XRCMD(xrStringToPath(gAppState.Instance, "/user/hand/left/input/back/click", &backPath[SIDE_LEFT]));
    CHECK_XRCMD(xrStringToPath(gAppState.Instance, "/user/hand/right/input/back/click", &backPath[SIDE_RIGHT]));
    CHECK_XRCMD(xrStringToPath(gAppState.Instance, "/user/hand/left/input/battery/value", &batteryPath[SIDE_LEFT]));
    CHECK_XRCMD(xrStringToPath(gAppState.Instance, "/user/hand/right/input/battery/value", &batteryPath[SIDE_RIGHT]));

    CHECK_XRCMD(xrStringToPath(gAppState.Instance, "/user/hand/left/input/x/click", &XValuePath[SIDE_LEFT]));
    CHECK_XRCMD(xrStringToPath(gAppState.Instance, "/user/hand/left/input/y/click", &YValuePath[SIDE_LEFT]));
    CHECK_XRCMD(xrStringToPath(gAppState.Instance, "/user/hand/right/input/a/click", &AValuePath[SIDE_RIGHT]));
    CHECK_XRCMD(xrStringToPath(gAppState.Instance, "/user/hand/right/input/b/click", &BValuePath[SIDE_RIGHT]));
    CHECK_XRCMD(xrStringToPath(gAppState.Instance, "/user/hand/left/input/x/touch", &XTouchPath[SIDE_LEFT]));
    CHECK_XRCMD(xrStringToPath(gAppState.Instance, "/user/hand/left/input/y/touch", &YTouchPath[SIDE_LEFT]));
    CHECK_XRCMD(xrStringToPath(gAppState.Instance, "/user/hand/right/input/a/touch", &ATouchPath[SIDE_RIGHT]));
    CHECK_XRCMD(xrStringToPath(gAppState.Instance, "/user/hand/right/input/b/touch", &BTouchPath[SIDE_RIGHT]));


    XrResult result;

    //First try Pico Devices
    {
        XrPath picoMixedRealityInteractionProfilePath;
        CHECK_XRCMD(xrStringToPath(gAppState.Instance, "/interaction_profiles/pico/neo3_controller",
                                   &picoMixedRealityInteractionProfilePath));

        XrActionSuggestedBinding bindings[128];
        int currBinding = 0;
        bindings[currBinding++] = ActionSuggestedBinding(touchpadAction, thumbstickClickPath[SIDE_LEFT]);
        bindings[currBinding++] = ActionSuggestedBinding(touchpadAction, thumbstickClickPath[SIDE_RIGHT]);
        bindings[currBinding++] = ActionSuggestedBinding(joystickAction, thumbstickPosPath[SIDE_LEFT]);
        bindings[currBinding++] = ActionSuggestedBinding(joystickAction, thumbstickPosPath[SIDE_RIGHT]);
        bindings[currBinding++] = ActionSuggestedBinding(thumbstickTouchAction, thumbstickTouchPath[SIDE_LEFT]);
        bindings[currBinding++] = ActionSuggestedBinding(thumbstickTouchAction, thumbstickTouchPath[SIDE_RIGHT]);

        bindings[currBinding++] = ActionSuggestedBinding(triggerAction, triggerValuePath[SIDE_LEFT]);
        bindings[currBinding++] = ActionSuggestedBinding(triggerAction, triggerValuePath[SIDE_RIGHT]);
        bindings[currBinding++] = ActionSuggestedBinding(TriggerTouchAction, triggerTouchPath[SIDE_LEFT]);
        bindings[currBinding++] = ActionSuggestedBinding(TriggerTouchAction, triggerTouchPath[SIDE_RIGHT]);

        bindings[currBinding++] = ActionSuggestedBinding(sideAction, squeezeClickPath[SIDE_LEFT]);
        bindings[currBinding++] = ActionSuggestedBinding(sideAction, squeezeClickPath[SIDE_RIGHT]);
        bindings[currBinding++] = ActionSuggestedBinding(GripAction, squeezeValuePath[SIDE_LEFT]);
        bindings[currBinding++] = ActionSuggestedBinding(GripAction, squeezeValuePath[SIDE_RIGHT]);
        bindings[currBinding++] = ActionSuggestedBinding(poseAction, posePath[SIDE_LEFT]);
        bindings[currBinding++] = ActionSuggestedBinding(poseAction, posePath[SIDE_RIGHT]);

        bindings[currBinding++] = ActionSuggestedBinding(homeAction, systemPath[SIDE_LEFT]);
        bindings[currBinding++] = ActionSuggestedBinding(homeAction, systemPath[SIDE_RIGHT]);
        bindings[currBinding++] = ActionSuggestedBinding(backAction, backPath[SIDE_LEFT]);
        bindings[currBinding++] = ActionSuggestedBinding(backAction, backPath[SIDE_RIGHT]);
        bindings[currBinding++] = ActionSuggestedBinding(batteryAction, batteryPath[SIDE_LEFT]);
        bindings[currBinding++] = ActionSuggestedBinding(batteryAction, batteryPath[SIDE_RIGHT]);

        bindings[currBinding++] = ActionSuggestedBinding(ThumbrestTouchAction, thumbrestPath[SIDE_LEFT]);
        bindings[currBinding++] = ActionSuggestedBinding(ThumbrestTouchAction, thumbrestPath[SIDE_RIGHT]);
        bindings[currBinding++] = ActionSuggestedBinding(vibrateAction, hapticPath[SIDE_LEFT]);
        bindings[currBinding++] = ActionSuggestedBinding(vibrateAction, hapticPath[SIDE_RIGHT]);

        bindings[currBinding++] = ActionSuggestedBinding(XTouchAction, XTouchPath[SIDE_LEFT]);
        bindings[currBinding++] = ActionSuggestedBinding(YTouchAction, YTouchPath[SIDE_LEFT]);
        bindings[currBinding++] = ActionSuggestedBinding(ATouchAction, ATouchPath[SIDE_RIGHT]);
        bindings[currBinding++] = ActionSuggestedBinding(BTouchAction, BTouchPath[SIDE_RIGHT]);
        bindings[currBinding++] = ActionSuggestedBinding(XAction, XValuePath[SIDE_LEFT]);
        bindings[currBinding++] = ActionSuggestedBinding(YAction, YValuePath[SIDE_LEFT]);
        bindings[currBinding++] = ActionSuggestedBinding(AAction, AValuePath[SIDE_RIGHT]);
        bindings[currBinding++] = ActionSuggestedBinding(BAction, BValuePath[SIDE_RIGHT]);
        bindings[currBinding++] = ActionSuggestedBinding(aimAction, aimPath[SIDE_LEFT]);
        bindings[currBinding++] = ActionSuggestedBinding(aimAction, aimPath[SIDE_RIGHT]);

        XrInteractionProfileSuggestedBinding suggestedBindings = {};
        suggestedBindings.type = XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING;
        suggestedBindings.interactionProfile = picoMixedRealityInteractionProfilePath;
        suggestedBindings.suggestedBindings = bindings;
        suggestedBindings.countSuggestedBindings = currBinding;
        suggestedBindings.next = NULL;
        result = xrSuggestInteractionProfileBindings(gAppState.Instance, &suggestedBindings);
    }

    if (result != XR_SUCCESS)
    {
        XrPath touchControllerInteractionProfilePath;
        CHECK_XRCMD(xrStringToPath(gAppState.Instance, "/interaction_profiles/oculus/touch_controller",
                                   &touchControllerInteractionProfilePath));

        XrActionSuggestedBinding bindings[128];
        int currBinding = 0;
        bindings[currBinding++] = ActionSuggestedBinding(touchpadAction, thumbstickClickPath[SIDE_LEFT]);
        bindings[currBinding++] = ActionSuggestedBinding(touchpadAction, thumbstickClickPath[SIDE_RIGHT]);
        bindings[currBinding++] = ActionSuggestedBinding(joystickAction, thumbstickPosPath[SIDE_LEFT]);
        bindings[currBinding++] = ActionSuggestedBinding(joystickAction, thumbstickPosPath[SIDE_RIGHT]);
        bindings[currBinding++] = ActionSuggestedBinding(thumbstickTouchAction, thumbstickTouchPath[SIDE_LEFT]);
        bindings[currBinding++] = ActionSuggestedBinding(thumbstickTouchAction, thumbstickTouchPath[SIDE_RIGHT]);

        bindings[currBinding++] = ActionSuggestedBinding(triggerAction, triggerValuePath[SIDE_LEFT]);
        bindings[currBinding++] = ActionSuggestedBinding(triggerAction, triggerValuePath[SIDE_RIGHT]);
        bindings[currBinding++] = ActionSuggestedBinding(TriggerTouchAction, triggerTouchPath[SIDE_LEFT]);
        bindings[currBinding++] = ActionSuggestedBinding(TriggerTouchAction, triggerTouchPath[SIDE_RIGHT]);

        bindings[currBinding++] = ActionSuggestedBinding(GripAction, squeezeValuePath[SIDE_LEFT]);
        bindings[currBinding++] = ActionSuggestedBinding(GripAction, squeezeValuePath[SIDE_RIGHT]);
        bindings[currBinding++] = ActionSuggestedBinding(poseAction, posePath[SIDE_LEFT]);
        bindings[currBinding++] = ActionSuggestedBinding(poseAction, posePath[SIDE_RIGHT]);

        bindings[currBinding++] = ActionSuggestedBinding(backAction, menuClickPath[SIDE_LEFT]);

        bindings[currBinding++] = ActionSuggestedBinding(ThumbrestTouchAction, thumbrestPath[SIDE_LEFT]);
        bindings[currBinding++] = ActionSuggestedBinding(ThumbrestTouchAction, thumbrestPath[SIDE_RIGHT]);
        bindings[currBinding++] = ActionSuggestedBinding(vibrateAction, hapticPath[SIDE_LEFT]);
        bindings[currBinding++] = ActionSuggestedBinding(vibrateAction, hapticPath[SIDE_RIGHT]);

        bindings[currBinding++] = ActionSuggestedBinding(XTouchAction, XTouchPath[SIDE_LEFT]);
        bindings[currBinding++] = ActionSuggestedBinding(YTouchAction, YTouchPath[SIDE_LEFT]);
        bindings[currBinding++] = ActionSuggestedBinding(ATouchAction, ATouchPath[SIDE_RIGHT]);
        bindings[currBinding++] = ActionSuggestedBinding(BTouchAction, BTouchPath[SIDE_RIGHT]);
        bindings[currBinding++] = ActionSuggestedBinding(XAction, XValuePath[SIDE_LEFT]);
        bindings[currBinding++] = ActionSuggestedBinding(YAction, YValuePath[SIDE_LEFT]);
        bindings[currBinding++] = ActionSuggestedBinding(AAction, AValuePath[SIDE_RIGHT]);
        bindings[currBinding++] = ActionSuggestedBinding(BAction, BValuePath[SIDE_RIGHT]);
        bindings[currBinding++] = ActionSuggestedBinding(aimAction, aimPath[SIDE_LEFT]);
        bindings[currBinding++] = ActionSuggestedBinding(aimAction, aimPath[SIDE_RIGHT]);

        XrInteractionProfileSuggestedBinding suggestedBindings = {};
        suggestedBindings.type = XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING;
        suggestedBindings.interactionProfile = touchControllerInteractionProfilePath;
        suggestedBindings.suggestedBindings = bindings;
        suggestedBindings.countSuggestedBindings = currBinding;
        suggestedBindings.next = NULL;
        result = xrSuggestInteractionProfileBindings(gAppState.Instance, &suggestedBindings);
    }

    // PC branch: say what the runtime made of Team Beef's own suggestions.
    TBXR_Log("VR: Pico / Quest Touch controller bindings %s (result %d)\n",
             result == XR_SUCCESS ? "accepted" : "REFUSED", (int)result);

    /*
        PC branch: the Valve Index controller, suggested on its own.

        Not in Team Beef's chain above, which stops at the first profile the
        runtime accepts and so can only ever suggest one. OpenXR lets an app
        suggest bindings for every profile it knows and the runtime uses the
        one matching the hands it has; this is a separate call, so it cannot
        change what a Quest or Pico player gets.

        Laid out to match the Quest controls: Index A and B stand in for X
        and Y on the left hand, A and B on the right. The Index has no menu
        button and no thumbrest, so the two things a player could not reach -
        the pause menu and Duke's quick kick - go on the trackpads, which were
        unused. Pressing counts, not touching: force, thresholded by the
        runtime, so resting a thumb there does nothing.

            left trackpad press    pause menu
            right trackpad press   quick kick

        Refused by a runtime that does not know the profile, which is logged
        and changes nothing - SteamVR's remap of the Quest layout carries on.
    */
    if (VR_IndexBindings())
    {
        auto P = [](const char *s) { XrPath x = XR_NULL_PATH; xrStringToPath(gAppState.Instance, s, &x); return x; };
        gIndexProfilePath = P("/interaction_profiles/valve/index_controller");

        XrActionSuggestedBinding b[64];
        int n = 0;
        const char *hand[2] = { "/user/hand/left", "/user/hand/right" };
        char path[128];
        for (int side = 0; side < SIDE_COUNT; side++)
        {
            auto H = [&](const char *c) { snprintf(path, sizeof(path), "%s%s", hand[side], c); return P(path); };
            b[n++] = ActionSuggestedBinding(touchpadAction,        H("/input/thumbstick/click"));
            b[n++] = ActionSuggestedBinding(joystickAction,        H("/input/thumbstick"));
            b[n++] = ActionSuggestedBinding(thumbstickTouchAction, H("/input/thumbstick/touch"));
            b[n++] = ActionSuggestedBinding(triggerAction,         H("/input/trigger/value"));
            b[n++] = ActionSuggestedBinding(TriggerTouchAction,    H("/input/trigger/touch"));
            b[n++] = ActionSuggestedBinding(GripAction,            H("/input/squeeze/value"));
            b[n++] = ActionSuggestedBinding(poseAction,            H("/input/grip/pose"));
            b[n++] = ActionSuggestedBinding(aimAction,             H("/input/aim/pose"));
            b[n++] = ActionSuggestedBinding(vibrateAction,         H("/output/haptic"));
        }
        b[n++] = ActionSuggestedBinding(backAction,           P("/user/hand/left/input/trackpad/force"));
        b[n++] = ActionSuggestedBinding(ThumbrestTouchAction, P("/user/hand/right/input/trackpad/force"));
        b[n++] = ActionSuggestedBinding(XAction,      P("/user/hand/left/input/a/click"));
        b[n++] = ActionSuggestedBinding(XTouchAction, P("/user/hand/left/input/a/touch"));
        b[n++] = ActionSuggestedBinding(YAction,      P("/user/hand/left/input/b/click"));
        b[n++] = ActionSuggestedBinding(YTouchAction, P("/user/hand/left/input/b/touch"));
        b[n++] = ActionSuggestedBinding(AAction,      P("/user/hand/right/input/a/click"));
        b[n++] = ActionSuggestedBinding(ATouchAction, P("/user/hand/right/input/a/touch"));
        b[n++] = ActionSuggestedBinding(BAction,      P("/user/hand/right/input/b/click"));
        b[n++] = ActionSuggestedBinding(BTouchAction, P("/user/hand/right/input/b/touch"));

        XrInteractionProfileSuggestedBinding sb = {};
        sb.type = XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING;
        sb.interactionProfile = gIndexProfilePath;
        sb.suggestedBindings = b;
        sb.countSuggestedBindings = n;
        const XrResult ir = xrSuggestInteractionProfileBindings(gAppState.Instance, &sb);
        TBXR_Log("VR: vr_index_bindings ON - Valve Index controller bindings %s (result %d, %d bindings)\n",
                 ir == XR_SUCCESS ? "accepted" : "REFUSED", (int)ir, n);
    }
    else
    {
        TBXR_Log("VR: vr_index_bindings off - Index controllers get SteamVR's remap of the Quest layout\n");
    }

    XrActionSpaceCreateInfo actionSpaceInfo = {};
    actionSpaceInfo.type = XR_TYPE_ACTION_SPACE_CREATE_INFO;
    actionSpaceInfo.action = poseAction;
    actionSpaceInfo.poseInActionSpace.orientation.w = 1.f;
    actionSpaceInfo.subactionPath = handSubactionPath[SIDE_LEFT];
    CHECK_XRCMD(xrCreateActionSpace(gAppState.Session, &actionSpaceInfo, &handSpace[SIDE_LEFT]));
    actionSpaceInfo.subactionPath = handSubactionPath[SIDE_RIGHT];
    CHECK_XRCMD(xrCreateActionSpace(gAppState.Session, &actionSpaceInfo, &handSpace[SIDE_RIGHT]));
    actionSpaceInfo.action = aimAction;
    actionSpaceInfo.poseInActionSpace.orientation.w = 1.f;
    actionSpaceInfo.subactionPath = handSubactionPath[SIDE_LEFT];
    CHECK_XRCMD(xrCreateActionSpace(gAppState.Session, &actionSpaceInfo, &aimSpace[SIDE_LEFT]));
    actionSpaceInfo.subactionPath = handSubactionPath[SIDE_RIGHT];
    actionSpaceInfo.next = NULL;
    CHECK_XRCMD(xrCreateActionSpace(gAppState.Session, &actionSpaceInfo, &aimSpace[SIDE_RIGHT]));

    XrSessionActionSetsAttachInfo attachInfo = {};
    attachInfo.type = XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO;
    attachInfo.countActionSets = 1;
    attachInfo.actionSets = &actionSet;
    attachInfo.next = NULL;
    CHECK_XRCMD(xrAttachSessionActionSets(gAppState.Session, &attachInfo));
}

/*
    PC branch: true while the runtime is driving our Index bindings for the
    right hand. The quick kick on the thumbrest is gated to Quest headsets,
    because on other controllers resting a thumb would fire it; on an Index
    it is bound to a trackpad PRESS, which is deliberate, so it may fire.
*/
bool TBXR_IndexControllersActive()
{
    if (gIndexProfilePath == XR_NULL_PATH || gAppState.Session == XR_NULL_HANDLE) return false;
    XrPath right = XR_NULL_PATH;
    xrStringToPath(gAppState.Instance, "/user/hand/right", &right);
    XrInteractionProfileState st = { XR_TYPE_INTERACTION_PROFILE_STATE };
    if (xrGetCurrentInteractionProfile(gAppState.Session, right, &st) != XR_SUCCESS) return false;
    return st.interactionProfile == gIndexProfilePath;
}

void TBXR_SyncActions( void )
{
    if (actionSet)
    {
        XrActiveActionSet activeActionSet = {};
        activeActionSet.actionSet = actionSet;
        activeActionSet.subactionPath = XR_NULL_PATH;
        XrActionsSyncInfo syncInfo;
        syncInfo.type = XR_TYPE_ACTIONS_SYNC_INFO;
        syncInfo.countActiveActionSets = 1;
        syncInfo.activeActionSets = &activeActionSet;
        syncInfo.next = NULL;
        CHECK_XRCMD(xrSyncActions(gAppState.Session, &syncInfo));
    }
}

void TBXR_UpdateControllers( )
{
    TBXR_SyncActions();

    //get controller poses
    for (int i = 0; i < 2; i++) {
        XrSpaceVelocity vel = {};
        vel.type = XR_TYPE_SPACE_VELOCITY;
        XrSpaceLocation loc = {};
        loc.type = XR_TYPE_SPACE_LOCATION;
        loc.next = &vel;
        XrResult res = xrLocateSpace(aimSpace[i], gAppState.CurrentSpace, gAppState.FrameState.predictedDisplayTime, &loc);
        if (res != XR_SUCCESS) {
            ALOGE("xrLocateSpace error: %d", (int)res);
        }

        gAppState.TrackedController[i].Active = (loc.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) != 0;
        gAppState.TrackedController[i].Pose = loc.pose;
        gAppState.TrackedController[i].Velocity = vel;
    }

    leftRemoteTracking_new = gAppState.TrackedController[0];
    rightRemoteTracking_new = gAppState.TrackedController[1];


    //button mapping
    leftTrackedRemoteState_new.Buttons = 0;
    leftTrackedRemoteState_new.Touches = 0;
    if (GetActionStateBoolean(backAction, SIDE_LEFT).currentState) leftTrackedRemoteState_new.Buttons |= xrButton_Enter;
    if (GetActionStateBoolean(XAction, SIDE_LEFT).currentState) leftTrackedRemoteState_new.Buttons |= xrButton_X;
    if (GetActionStateBoolean(XTouchAction, SIDE_LEFT).currentState) leftTrackedRemoteState_new.Touches |= xrButton_X;
    if (GetActionStateBoolean(YAction, SIDE_LEFT).currentState) leftTrackedRemoteState_new.Buttons |= xrButton_Y;
    if (GetActionStateBoolean(YTouchAction, SIDE_LEFT).currentState) leftTrackedRemoteState_new.Touches |= xrButton_Y;
    leftTrackedRemoteState_new.GripTrigger = GetActionStateFloat(GripAction, SIDE_LEFT).currentState;
    if (leftTrackedRemoteState_new.GripTrigger > 0.5f) leftTrackedRemoteState_new.Buttons |= xrButton_GripTrigger;
    if (GetActionStateBoolean(touchpadAction, SIDE_LEFT).currentState) leftTrackedRemoteState_new.Buttons |= xrButton_LThumb;
    if (GetActionStateBoolean(touchpadAction, SIDE_LEFT).currentState) leftTrackedRemoteState_new.Buttons |= xrButton_Joystick;
    if (GetActionStateBoolean(thumbstickTouchAction, SIDE_LEFT).currentState) leftTrackedRemoteState_new.Touches |= xrButton_LThumb;
    if (GetActionStateBoolean(thumbstickTouchAction, SIDE_LEFT).currentState) leftTrackedRemoteState_new.Touches |= xrButton_Joystick;
    leftTrackedRemoteState_new.IndexTrigger = GetActionStateFloat(triggerAction, SIDE_LEFT).currentState;
    if (leftTrackedRemoteState_new.IndexTrigger > 0.5f) leftTrackedRemoteState_new.Buttons |= xrButton_Trigger;
    if (GetActionStateBoolean(TriggerTouchAction, SIDE_LEFT).currentState) leftTrackedRemoteState_new.Touches |= xrButton_Trigger;
    if (GetActionStateBoolean(ThumbrestTouchAction, SIDE_LEFT).currentState) leftTrackedRemoteState_new.Touches |= xrButton_ThumbRest;

    rightTrackedRemoteState_new.Buttons = 0;
    rightTrackedRemoteState_new.Touches = 0;
    if (GetActionStateBoolean(backAction, SIDE_RIGHT).currentState) rightTrackedRemoteState_new.Buttons |= xrButton_Enter;
    if (GetActionStateBoolean(AAction, SIDE_RIGHT).currentState) rightTrackedRemoteState_new.Buttons |= xrButton_A;
    if (GetActionStateBoolean(ATouchAction, SIDE_RIGHT).currentState) rightTrackedRemoteState_new.Touches |= xrButton_A;
    if (GetActionStateBoolean(BAction, SIDE_RIGHT).currentState) rightTrackedRemoteState_new.Buttons |= xrButton_B;
    if (GetActionStateBoolean(BTouchAction, SIDE_RIGHT).currentState) rightTrackedRemoteState_new.Touches |= xrButton_B;
    rightTrackedRemoteState_new.GripTrigger = GetActionStateFloat(GripAction, SIDE_RIGHT).currentState;
    if (rightTrackedRemoteState_new.GripTrigger > 0.5f) rightTrackedRemoteState_new.Buttons |= xrButton_GripTrigger;
    if (GetActionStateBoolean(touchpadAction, SIDE_RIGHT).currentState) rightTrackedRemoteState_new.Buttons |= xrButton_RThumb;
    if (GetActionStateBoolean(touchpadAction, SIDE_RIGHT).currentState) rightTrackedRemoteState_new.Buttons |= xrButton_Joystick;
    if (GetActionStateBoolean(thumbstickTouchAction, SIDE_RIGHT).currentState) rightTrackedRemoteState_new.Touches |= xrButton_RThumb;
    if (GetActionStateBoolean(thumbstickTouchAction, SIDE_RIGHT).currentState) rightTrackedRemoteState_new.Touches |= xrButton_Joystick;
    rightTrackedRemoteState_new.IndexTrigger = GetActionStateFloat(triggerAction, SIDE_RIGHT).currentState;
    if (rightTrackedRemoteState_new.IndexTrigger > 0.5f) rightTrackedRemoteState_new.Buttons |= xrButton_Trigger;
    if (GetActionStateBoolean(TriggerTouchAction, SIDE_RIGHT).currentState) rightTrackedRemoteState_new.Touches |= xrButton_Trigger;
    if (GetActionStateBoolean(ThumbrestTouchAction, SIDE_RIGHT).currentState) rightTrackedRemoteState_new.Touches |= xrButton_ThumbRest;

    //thumbstick
    XrActionStateVector2f moveJoystickState;
    moveJoystickState = GetActionStateVector2(joystickAction, SIDE_LEFT);
    leftTrackedRemoteState_new.Joystick.x = moveJoystickState.currentState.x;
    leftTrackedRemoteState_new.Joystick.y = moveJoystickState.currentState.y;

    moveJoystickState = GetActionStateVector2(joystickAction, SIDE_RIGHT);
    rightTrackedRemoteState_new.Joystick.x = moveJoystickState.currentState.x;
    rightTrackedRemoteState_new.Joystick.y = moveJoystickState.currentState.y;
}


//0 = left, 1 = right
float vibration_channel_duration[2] = {0.0f, 0.0f};
float vibration_channel_intensity[2] = {0.0f, 0.0f};

void TBXR_Vibrate( int duration, int chan, float intensity )
{
    for (int i = 0; i < 2; ++i)
    {
        int channel = 1-i;
        if ((i + 1) & chan)
        {
            if (vibration_channel_duration[channel] > 0.0f)
                return;

            if (vibration_channel_duration[channel] == -1.0f && duration != 0.0f)
                return;

            vibration_channel_duration[channel] = duration;
            vibration_channel_intensity[channel] = intensity;
        }
    }
}

void TBXR_ProcessHaptics() {
    static float lastFrameTime = 0.0f;
    float timestamp = (float)(TBXR_GetTimeInMilliSeconds( ));
    float frametime = timestamp - lastFrameTime;
    lastFrameTime = timestamp;

    for (int i = 0; i < 2; ++i) {
        if (vibration_channel_duration[i] > 0.0f ||
            vibration_channel_duration[i] == -1.0f) {

            // fire haptics using output action
            XrHapticVibration vibration = {};
            vibration.type = XR_TYPE_HAPTIC_VIBRATION;
            vibration.next = NULL;
            vibration.amplitude = vibration_channel_intensity[i];
            vibration.duration = ToXrTime(vibration_channel_duration[i]);
            vibration.frequency = 3000;
            XrHapticActionInfo hapticActionInfo = {};
            hapticActionInfo.type = XR_TYPE_HAPTIC_ACTION_INFO;
            hapticActionInfo.next = NULL;
            hapticActionInfo.action = vibrateAction;
            hapticActionInfo.subactionPath = handSubactionPath[i];
            OXR(xrApplyHapticFeedback(gAppState.Session, &hapticActionInfo, (const XrHapticBaseHeader*)&vibration));

            if (vibration_channel_duration[i] != -1.0f) {
                vibration_channel_duration[i] -= frametime;

                if (vibration_channel_duration[i] < 0.0f) {
                    vibration_channel_duration[i] = 0.0f;
                    vibration_channel_intensity[i] = 0.0f;
                }
            }
        } else {
            // Stop haptics
            XrHapticActionInfo hapticActionInfo = {};
            hapticActionInfo.type = XR_TYPE_HAPTIC_ACTION_INFO;
            hapticActionInfo.next = NULL;
            hapticActionInfo.action = vibrateAction;
            hapticActionInfo.subactionPath = handSubactionPath[i];
            OXR(xrStopHapticFeedback(gAppState.Session, &hapticActionInfo));
        }
    }
}
