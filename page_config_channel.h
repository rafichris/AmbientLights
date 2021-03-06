#ifndef PAGE_CONFIG_CHANNEL_H_
#define PAGE_CONFIG_CHANNEL_H_

void send_config_channel_html(AsyncWebServerRequest *request) {
    if (request->params()) {
        config.channel_gamma = false;
        config.zero = false;
        config.ap = false;
        digitalWrite(PIN_D4, HIGH);
        uint16_t old_nb_chan = config.channel_count;
        for (uint8_t i = 0; i < request->params(); i++) {
            AsyncWebParameter *p = request->getParam(i);
            if (p->name() == "devid") config.id = p->value();
            if (p->name() == "maxVal") config.maxVal = p->value().toInt();
            if (p->name() == "interVal") config.interVal = p->value().toInt();
            if (p->name() == "slopeVal") config.slopeVal = p->value().toInt();
            if (p->name() == "channel_count") config.channel_count = p->value().toInt();
            if (p->name() == "gamma") config.channel_gamma = true;
            if (p->name() == "zero") config.zero = true;
            if (p->name() == "ap") config.ap = true;
        }

        // use default mappinf if number of channels got updated
        if(old_nb_chan != config.channel_count){
            if (mapping) free(mapping);
            mapping = static_cast<uint16_t *>(malloc(config.channel_count*2));
            for ( uint16_t i = 0; i < config.channel_count; i++){
                mapping[i] = i + 1;
            }
        }
        saveConfig();

        AsyncWebServerResponse *response = request->beginResponse(303);
        response->addHeader("Location", request->url());
        request->send(response);

        lSetupFinishedMillis = millis();
    } else {
        request->send(400);
    }
}

void send_config_channel_vals(AsyncWebServerRequest *request) {
    String values = "";
    values += "devid|input|" + config.id + "\n";
    values += "maxVal|input|" + String(config.maxVal) + "\n";
    values += "interVal|input|" + String(config.interVal) + "\n";
    values += "slopeVal|input|" + String(config.slopeVal) + "\n";
    values += "channel_count|input|" + String(config.channel_count) + "\n";
    values += "gamma|chk|" + String(config.channel_gamma ? "checked" : "") + "\n";
    values += "zero|chk|" + String(config.zero ? "checked" : "") + "\n";
    values += "ap|chk|" + String(config.ap ? "checked" : "") + "\n";
    values += "title|div|" + config.id + " - Channel Config\n";
    request->send(200, "text/plain", values);
}

#endif /* PAGE_CONFIG_CHANNEL_H_ */
