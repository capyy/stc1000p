#define	COM_PIN			D3	// ICSPCLK

#define COM_READ_EEPROM		0x20
#define COM_WRITE_EEPROM	0xE0
#define COM_READ_TEMP		0x01
#define COM_READ_COOLING	0x02
#define COM_READ_HEATING	0x03
#define COM_ACK			0x9A
#define COM_NACK		0x66


//char t1[10];  // temperature string

double t3 = 0.0;
int heatInt = 2;
int coolInt = 2;

unsigned long lastTime1 = 0; //timer 1
unsigned long lastTime2 = 0; //timer 2

void write_bit(unsigned const char data){
	pinMode(COM_PIN, OUTPUT);
	digitalWrite(COM_PIN, HIGH);
	delayMicroseconds(7);
	if(!data){
		pinMode(COM_PIN, INPUT);
		digitalWrite(COM_PIN, LOW);
	}
	delayMicroseconds(400);
	pinMode(COM_PIN, INPUT);
	digitalWrite(COM_PIN, LOW);
	delayMicroseconds(100);
}

unsigned char read_bit(){
	unsigned char data;

	pinMode(COM_PIN, OUTPUT);
	digitalWrite(COM_PIN, HIGH);
	delayMicroseconds(7);
	pinMode(COM_PIN, INPUT);
	digitalWrite(COM_PIN, LOW);
	delayMicroseconds(200);
	data = digitalRead(COM_PIN);
	delayMicroseconds(300);

	return data;
}

void write_byte(unsigned const char data){
	unsigned char i;

	for(i=0;i<8;i++){
		write_bit(((data << i) & 0x80));
	}
	delayMicroseconds(500);
}

unsigned char read_byte(){
	unsigned char i, data;

	for(i=0;i<8;i++){
		data <<= 1;
		if(read_bit()){
			data |= 1;
		}
	}
	delayMicroseconds(500);

	return data;
}


bool write_eeprom(const unsigned char address, unsigned const int value){
	unsigned char ack;
	write_byte(COM_WRITE_EEPROM);
	write_byte(address);
	write_byte(((unsigned char)(value >> 8)));
	write_byte((unsigned char)value);
	write_byte(COM_WRITE_EEPROM ^ address ^ ((unsigned char)(value >> 8)) ^ ((unsigned char)value));
	delay(6); // Longer delay needed here for EEPROM write to finish, but must be shorter than 10ms
	ack = read_byte();
	return ack == COM_ACK;
}

bool read_eeprom(const unsigned char address, int *value){
	unsigned char xorsum;
	unsigned char ack;
        unsigned int data;

	write_byte(COM_READ_EEPROM);
	write_byte(address);
	data = read_byte();
	data = (data << 8) | read_byte();
	xorsum = read_byte();
        ack = read_byte();
	if(ack == COM_ACK && xorsum == (COM_READ_EEPROM ^ address ^ ((unsigned char)(data >> 8)) ^ ((unsigned char)data))){
	        *value = (int)data;
		return true;
	}
	return false;
}

bool read_command(unsigned char command, int *value){
	unsigned char xorsum;
	unsigned char ack;
        unsigned int data;

	write_byte(command);
	data = read_byte();
	data = (data << 8) | read_byte();
	xorsum = read_byte();
        ack = read_byte();
	if(ack == COM_ACK && xorsum == (command ^ ((unsigned char)(data >> 8)) ^ ((unsigned char)data))){
	        *value = (int)data;
		return true;
	}
	return false;
}

bool read_temp(int *temperature){
	return read_command(COM_READ_TEMP, temperature);
}

bool read_heating(int *heating){
	return read_command(COM_READ_HEATING, heating);
}

bool read_cooling(int *cooling){
	return read_command(COM_READ_COOLING, cooling);
}

/* End of communication implementation */

/* From here example implementation begins, this can be exchanged for your specific needs */
enum set_menu_enum {
	setpoint,			// SP (setpoint)
	hysteresis,			// hy (hysteresis)
	temperature_correction,		// tc (temperature correction)
	setpoint_alarm,			// SA (setpoint alarm)
	step,				// St (current running profile step)
	duration,			// dh (current running profile step duration in hours)
	cooling_delay,			// cd (cooling delay minutes)
	heating_delay,			// hd (heating delay minutes)
	ramping,			// rP (0=disable, 1=enable ramping)
	run_mode			// rn (0-5 run profile, 6=thermostat)
};

/* Defines for EEPROM config addresses */
#define EEADR_PROFILE_SETPOINT(profile, stp)	(((profile)*19) + ((stp)<<1))
#define EEADR_PROFILE_DURATION(profile, stp)	(EEADR_PROFILE_SETPOINT(profile, stp) + 1)
#define EEADR_SET_MENU				EEADR_PROFILE_SETPOINT(6, 0)
#define EEADR_SET_MENU_ITEM(name)		(EEADR_SET_MENU + (name))
#define EEADR_POWER_ON				127

const char menu_opt[][4] = {
	"SP",
	"hy",
	"tc",
	"SA",
	"St",
	"dh",
	"cd",
	"hd",
	"rP",
	"rn"
};

bool isBlank(char c){
	return c == ' ' || c == '\t';
}

bool isDigit(char c){
	return c >= '0' && c <= '9';
}

bool isEOL(char c){
	return c == '\r' || c == '\n';
}

void print_temperature(int temperature){
	if(temperature < 0){
		temperature = -temperature;
		Serial.print('-');
	}
	if(temperature >= 1000){
		temperature /= 10;
		Serial.println(temperature);
	} else {
		Serial.print(temperature/10);
		Serial.print('.');
		Serial.println(temperature%10);
	}
}

void print_config_value(unsigned char address, int value){
	if(address < EEADR_SET_MENU){
		unsigned char profile=0;
		while(address >= 19){
			address-=19;
			profile++;
		}
		if(address & 1){
			Serial.print("dh");
		} else {
			Serial.print("SP");
		}
		Serial.print(profile);
		Serial.print(address >> 1);
		Serial.print('=');
		if(address & 1){
			Serial.println(value);
		} else {
			print_temperature(value);
		}
	} else {
		Serial.print(menu_opt[address-EEADR_SET_MENU]);
		Serial.print('=');
		if(address == EEADR_SET_MENU_ITEM(run_mode)){
			if(value >= 0 && value <= 5){
				Serial.print("Pr");
				Serial.println(value);
			} else {
				Serial.println("th");
			}
		} else if(address <= EEADR_SET_MENU_ITEM(setpoint_alarm)){
			print_temperature(value);
		} else {
			Serial.println(value);
		}
	}
}

unsigned char parse_temperature(const char *str, int *temperature){
	unsigned char i=0;
	bool neg = false;

	if(str[i] == '-'){
		neg = true;
		i++;
	}

	if(!isDigit(str[i])){
		return 0;
	}

	*temperature = 0;
	while(isDigit(str[i])){
		*temperature = *temperature * 10 + (str[i] - '0');
		i++;
	}
	*temperature *= 10;
	if(str[i] == '.'){
		i++;
		if(isDigit(str[i])){
			*temperature += (str[i] - '0');
			i++;
		} else {
			return 0;
		}
	}

	if(neg){
		*temperature = -(*temperature);
	}

	return i;
}

unsigned char parse_address(const char *cmd, unsigned char *addr){
	char i;

	if(!strncmp("SP", cmd, 2)){
		if(isDigit(cmd[2]) && isDigit(cmd[3]) && cmd[2] < '6'){
			*addr = EEADR_PROFILE_SETPOINT(cmd[2]-'0', cmd[3]-'0');
			return 4;
		}
	}

	if(!strncmp("dh", cmd, 2)){
		if(isDigit(cmd[2]) && isDigit(cmd[3]) && cmd[2] < '6' && cmd[3] < '9'){
			*addr = EEADR_PROFILE_DURATION(cmd[2]-'0', cmd[3]-'0');
			return 4;
		}
	}

	for(i=0; i<(sizeof(menu_opt)/sizeof(menu_opt[0])); i++){
		unsigned char len = strlen(menu_opt[i]);
		if(!strncmp(cmd, &menu_opt[i][0], len) && (isBlank(cmd[len]) || isEOL(cmd[len]))){
			*addr = EEADR_SET_MENU + i;
			return strlen(menu_opt[i]);
		}
	}

	*addr = 0;
	for(i=0; i<30; i++){
		if(isBlank(cmd[i]) || isEOL(cmd[i])){
			break;
		}
		if(isDigit(cmd[i])){
			if(*addr>12){
				return 0;
			} else {
				*addr = *addr * 10 + (cmd[i] - '0');
		 	}
		} else {
			return 0;
		}
	}

	if(*addr > 127){
		return 0;
	}

	return i;
}

unsigned char parse_config_value(const char *cmd, int address, bool pretty, int *data){
	unsigned char i=0;
	bool neg=false;

	if(pretty){
		if(address < EEADR_SET_MENU){
			while(address >= 19){
				address-=19;
			}
			if((address & 1) == 0){
				return parse_temperature(cmd, data);
			}
		} else if(address <= EEADR_SET_MENU_ITEM(setpoint_alarm)){
			return parse_temperature(cmd, data);
		} else if(address == EEADR_SET_MENU_ITEM(run_mode)) {
			if(!strncmp(cmd, "Pr", 2)){
				*data = cmd[2] - '0';
				if(*data >= 0 && *data <= 5){
					return 3;
				}
			} else if(!strncmp(cmd, "th", 2)){
				*data = 6;
				return 2;
			}
			return 0;
		}
	}

	if(cmd[i] == '-'){
		neg = true;
		i++;
	}

	if(!isDigit(cmd[i])){
		return 0;
	}

	for(*data=0; i<6; i++){
		if(!isDigit(cmd[i])){
			break;
		}
		if(isDigit(cmd[i]) && *data < 3276){
			*data = *data * 10 + (cmd[i] - '0');
		} else {
			return 0;
		}
	}

	if((neg && *data > 32768) || (!neg && *data > 32767)){
		return 0;
	}

	if(neg){
		*data = -(*data);
	}

	return i;
}

void parse_command(char *cmd){
	int data;

	if(cmd[0] == 't'){
		if(!isEOL(cmd[1])){
			Serial.println("?Syntax error");
			return;
		}
		if(read_temp(&data)){
			Serial.print("Temperature=");
			print_temperature(data);
		} else {
			Serial.println("?Communication error");
		}
	} else if(cmd[0] == 'h'){
		if(!isEOL(cmd[1])){
			Serial.println("?Syntax error");
			return;
		}
		if(read_heating(&data)){
			Serial.print("Heating=");
			Serial.println(data ? "on" : "off");
		} else {
			Serial.println("?Communication error");
		}
	} else if(cmd[0] == 'c'){
		if(!isEOL(cmd[1])){
			Serial.println("?Syntax error");
			return;
		}
		if(read_cooling(&data)){
			Serial.print("Cooling=");
			Serial.println(data ? "on" : "off");
		} else {
			Serial.println("?Communication error");
		}
	} else if(cmd[0] == 'r' || cmd[0] == 'w') {
		unsigned char address=0;
		unsigned char i=0, j;
		bool neg = false;

		if(!isBlank(cmd[1])){
			Serial.println("?Syntax error");
			return;
		}

		j = parse_address(&cmd[2], &address);
		i+=j+2;

		if(j==0){
			Serial.println("?Syntax error");
			return;
		}

		if(cmd[0] == 'r'){
			if(!isEOL(cmd[i])){
				Serial.println("?Syntax error");
				return;
			}
			if(read_eeprom(address, &data)){
				if(isDigit(cmd[2])){
					Serial.print("EEPROM[");
					Serial.print(address);
					Serial.print("]=");
					Serial.println(data);
				} else {
					print_config_value(address, data);
				}
			} else {
				Serial.println("?Communication error");
			}
			return;
		}

		if(!isBlank(cmd[i])){
			Serial.println("?Syntax error");
			return;
		}
		i++;

		j = parse_config_value(&cmd[i], address, !isDigit(cmd[2]), &data);
		i += j;
		if(j == 0){
			Serial.println("?Syntax error");
			return;
		} else {
			if(!isEOL(cmd[i])){
				Serial.println("?Syntax error");
				return;
			}
			if(write_eeprom(address, data)){
				Serial.println("Ok");
			} else {
				Serial.println("?Communication error");
			}
		}
	}
}

void temp(){
    int temperature;
    String result = "";
	if(read_temp(&temperature)){


	    t3 = temperature / 10.0;
	    Particle.publish("temp_stc", String(t3), PRIVATE);
		if(temperature < 0){
            temperature = -temperature;
	        result = "-";
	    }
        if(temperature >= 1000){
            temperature /= 10;
            result = result + String(temperature);
	    } else {
		    result = result + String((temperature/10)) + "." + String((temperature%10));
	    }

	}
	//t1 = result;

}

void heat(){
    int isHeating;
    String heatTxt = "";
    if(read_heating(&isHeating)){
		if(isHeating != heatInt){
		    heatInt = isHeating;
		    if(heatInt == 1){
		        heatTxt = "1";
		    }else{
		        heatTxt = "0";
		    }
		    Particle.publish("heating_stc", heatTxt, PRIVATE);
		}
	}
}

void cool(){
    int isCooling;
    String coolTxt = "";
    if(read_cooling(&isCooling)){
		if(isCooling != coolInt){
		    coolInt = isCooling;
		    if(coolInt == 1){
		        coolTxt = "1";
		    }else{
		        coolTxt = "0";
		    }
		    Particle.publish("cooling_stc", coolTxt, PRIVATE);
		}
	}
}

void setup() {
	Serial.begin(115200);

	delay(2);
    //Particle.variable("tempString", t1, STRING);
    Particle.variable("tempInt", t3);
    Particle.variable("cooling", coolInt);
    Particle.variable("heating", heatInt);
    delay(2000);
    temp();
    delay(2000);
    heat();
    delay(2000);
    cool();
}

void loop() {

    unsigned long now = millis();

    //Temperature timer
	if ((now - lastTime1) >= 120000) { //read temp every 2 minutes
		lastTime1 = now;
        temp();
	}

    delay(2000);

    //Relay timer
	if ((now - lastTime2) >= 20000) { //Read relays every 20sec
		lastTime2 = now;
        heat();
        delay(2000);
        cool();
	}

}
