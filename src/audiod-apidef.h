
static const char _afb_description_v2_audiod[] =
    "{\"openapi\":\"3.0.0\",\"info\":{\"description\":\"Audio Backend API for"
    " AGL applications\",\"title\":\"audio backend\",\"version\":\"1.0\",\"x-"
    "binding-c-generator\":{\"api\":\"audiod\",\"version\":2,\"prefix\":\"aud"
    "iod_\",\"postfix\":\"\",\"start\":null,\"onevent\":\"AudioDOnEvent\",\"i"
    "nit\":\"AudioDBindingInit\",\"scope\":\"\",\"private\":false}},\"servers"
    "\":[{\"url\":\"ws://{host}:{port}/api/audiod\",\"description\":\"Audio B"
    "ackend API for AGL applications.\",\"variables\":{\"host\":{\"default\":"
    "\"localhost\"},\"port\":{\"default\":\"1234\"}},\"x-afb-events\":[{\"$re"
    "f\":\"#/components/schemas/afb-event\"}]}],\"components\":{\"schemas\":{"
    "\"afb-reply\":{\"$ref\":\"#/components/schemas/afb-reply-v2\"},\"afb-eve"
    "nt\":{\"$ref\":\"#/components/schemas/afb-event-v2\"},\"afb-reply-v2\":{"
    "\"title\":\"Generic response.\",\"type\":\"object\",\"required\":[\"jtyp"
    "e\",\"request\"],\"properties\":{\"jtype\":{\"type\":\"string\",\"const\""
    ":\"afb-reply\"},\"request\":{\"type\":\"object\",\"required\":[\"status\""
    "],\"properties\":{\"status\":{\"type\":\"string\"},\"info\":{\"type\":\""
    "string\"},\"token\":{\"type\":\"string\"},\"uuid\":{\"type\":\"string\"}"
    ",\"reqid\":{\"type\":\"string\"}}},\"response\":{\"type\":\"object\"}}},"
    "\"afb-event-v2\":{\"type\":\"object\",\"required\":[\"jtype\",\"event\"]"
    ",\"properties\":{\"jtype\":{\"type\":\"string\",\"const\":\"afb-event\"}"
    ",\"event\":{\"type\":\"string\"},\"data\":{\"type\":\"object\"}}}},\"res"
    "ponses\":{\"200\":{\"description\":\"A complex object array response\",\""
    "content\":{\"application/json\":{\"schema\":{\"$ref\":\"#/components/sch"
    "emas/afb-reply\"}}}},\"400\":{\"description\":\"Invalid arguments\"}}},\""
    "paths\":{\"/play\":{\"description\":\"Open an ALSA PCM Device name and P"
    "lay the associate media wave file\",\"get\":{\"parameters\":[{\"in\":\"q"
    "uery\",\"name\":\"device_name\",\"required\":true,\"schema\":{\"type\":\""
    "string\"}},{\"in\":\"query\",\"name\":\"filepath\",\"required\":true,\"s"
    "chema\":{\"type\":\"string\"}},{\"in\":\"query\",\"name\":\"loop\",\"req"
    "uired\":true,\"schema\":{\"type\":\"int\"}}],\"responses\":{\"200\":{\"$"
    "ref\":\"#/components/responses/200\"},\"400\":{\"$ref\":\"#/components/r"
    "esponses/400\"}}}},\"/stop\":{\"description\":\"Stop and close the playi"
    "ng Alsa Device\",\"get\":{\"responses\":{\"200\":{\"$ref\":\"#/component"
    "s/responses/200\"},\"400\":{\"$ref\":\"#/components/responses/400\"}}}},"
    "\"/pause\":{\"description\":\"Pause a playing stream\",\"get\":{\"respon"
    "ses\":{\"200\":{\"$ref\":\"#/components/responses/200\"},\"400\":{\"$ref"
    "\":\"#/components/responses/400\"}}}},\"/subscribe\":{\"description\":\""
    "Subscribe to audio events\",\"get\":{\"parameters\":[{\"in\":\"query\",\""
    "name\":\"events\",\"required\":true,\"schema\":{\"type\":\"array\",\"ite"
    "ms\":{\"type\":\"string\"}}}],\"responses\":{\"200\":{\"$ref\":\"#/compo"
    "nents/responses/200\"},\"400\":{\"$ref\":\"#/components/responses/400\"}"
    "}}},\"/unsubscribe\":{\"description\":\"Unubscribe to audio events\",\"g"
    "et\":{\"parameters\":[{\"in\":\"query\",\"name\":\"events\",\"required\""
    ":true,\"schema\":{\"type\":\"array\",\"items\":{\"type\":\"string\"}}}],"
    "\"responses\":{\"200\":{\"$ref\":\"#/components/responses/200\"},\"400\""
    ":{\"$ref\":\"#/components/responses/400\"}}}},\"/post_event\":{\"descrip"
    "tion\":\"Push an event to AGL\",\"get\":{\"parameters\":[{\"in\":\"query"
    "\",\"name\":\"event_name\",\"required\":true,\"schema\":{\"type\":\"stri"
    "ng\"}},{\"in\":\"query\",\"name\":\"event_parameter\",\"required\":true,"
    "\"schema\":{\"type\":\"object\",\"required\":[\"speed\"],\"properties\":"
    "{\"speed\":{\"type\":\"int\"}}}}],\"responses\":{\"200\":{\"$ref\":\"#/c"
    "omponents/responses/200\"},\"400\":{\"$ref\":\"#/components/responses/40"
    "0\"}}}}}}"
;

 void audiod_play(struct afb_req req);
 void audiod_stop(struct afb_req req);
 void audiod_pause(struct afb_req req);
 void audiod_subscribe(struct afb_req req);
 void audiod_unsubscribe(struct afb_req req);
 void audiod_post_event(struct afb_req req);

static const struct afb_verb_v2 _afb_verbs_v2_audiod[] = {
    {
        .verb = "play",
        .callback = audiod_play,
        .auth = NULL,
        .info = "Open an ALSA PCM Device name and Play the associate media wave file",
        .session = AFB_SESSION_NONE_V2
    },
    {
        .verb = "stop",
        .callback = audiod_stop,
        .auth = NULL,
        .info = "Stop and close the playing Alsa Device",
        .session = AFB_SESSION_NONE_V2
    },
    {
        .verb = "pause",
        .callback = audiod_pause,
        .auth = NULL,
        .info = "Pause a playing stream",
        .session = AFB_SESSION_NONE_V2
    },
    {
        .verb = "subscribe",
        .callback = audiod_subscribe,
        .auth = NULL,
        .info = "Subscribe to audio events",
        .session = AFB_SESSION_NONE_V2
    },
    {
        .verb = "unsubscribe",
        .callback = audiod_unsubscribe,
        .auth = NULL,
        .info = "Unubscribe to audio events",
        .session = AFB_SESSION_NONE_V2
    },
    {
        .verb = "post_event",
        .callback = audiod_post_event,
        .auth = NULL,
        .info = "Push an event to AGL",
        .session = AFB_SESSION_NONE_V2
    },
    {
        .verb = NULL,
        .callback = NULL,
        .auth = NULL,
        .info = NULL,
        .session = 0
	}
};

const struct afb_binding_v2 afbBindingV2 = {
    .api = "audiod",
    .specification = _afb_description_v2_audiod,
    .info = "Audio Backend API for AGL applications",
    .verbs = _afb_verbs_v2_audiod,
    .preinit = NULL,
    .init = AudioDBindingInit,
    .onevent = AudioDOnEvent,
    .noconcurrency = 0
};

