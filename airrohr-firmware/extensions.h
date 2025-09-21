#include "extensions_cfg.h"

static float dew_point(const float temperature, const float humidity);

static String dew_point_string(const float temperature, const float humidity)
{
	const float dew_point_temp = dew_point(temperature, humidity);
	return isnan(dew_point_temp) ? "-128.0" : String(dew_point_temp, 1);
}

// second sensor
BMX280 bmx280x;
bool bmx280x_init_failed = false;
TemperatureValue  last_value_BMX280x_T{"BME280x_temperature"};
FloatValue        last_value_BMX280x_P{"BME280x_pressure"};
HumidityValue     last_value_BME280x_H{"BME280x_humidity"};
// values from remote sensor (outside, inside)
time_t next_update_r = 0;
long count_measurements_r = -1;
int last_signal_strength_r = 0;
float last_value_BMX280_T_r = -128.0;
float last_value_BMX280_P_r = -1.0;
float last_value_BME280_H_r = -1.0;
float last_value_BMX280x_T_r = -128.0;
float last_value_BMX280x_P_r = -1.0;
float last_value_BME280x_H_r = -1.0;
float last_value_SDS_P1_r = -1.0;
float last_value_SDS_P2_r = -1.0;

/*****************************************************************
 * Init second BMP280/BME280                                     *
 *****************************************************************/
static bool initBMX280x(char addr)
{
	debug_out(String(F("Trying second BMx280 sensor on ")) + String(addr, HEX), DEBUG_MIN_INFO);

	if (bmx280x.begin(addr))
	{
		debug_outln_info(FPSTR(DBG_TXT_FOUND));
		bmx280x.setSampling(
			BMX280::MODE_FORCED,
			BMX280::SAMPLING_X1,
			BMX280::SAMPLING_X1,
			BMX280::SAMPLING_X1);
		return true;
	}
	else
	{
		debug_outln_info(FPSTR(DBG_TXT_NOT_FOUND));
		return false;
	}
}

/*****************************************************************
 * read second BMP280/BME280 sensor values                       *
 *****************************************************************/
static void fetchSensorBMX280x()
{
	bmx280x.takeForcedMeasurement();
	const auto t = bmx280x.readTemperature();
	const auto p = bmx280x.readPressure();
	const auto h = bmx280x.readHumidity();
	if (isnan(t) || isnan(p))
	{
		last_value_BMX280x_T = -128.0;
		last_value_BMX280x_P = -1.0;
		last_value_BME280x_H = -1.0;
		debug_outln_error(F("BMP/BME280x read failed"));
	}
	else
	{
		last_value_BMX280x_T = t + readCorrectionOffset(cfg::temp_correction);
		last_value_BMX280x_P = p;
		if (bmx280x.sensorID() == BME280_SENSOR_ID)
		{
			last_value_BME280x_H = h;
		}
	}
}

/*****************************************************************
 * Webserver xdata.json                                          *
 *****************************************************************/
static String get_xdata_json()
{
	RESERVE_STRING(json, XXLARGE_STR);
	json = String(FPSTR(data_first_part));
	{
		long age_ms = msSince(starttime);
		if (count_sends == 0)
		{
			age_ms -= cfg::sending_intervall_ms;
		}
		add_Value2Json(json, F("age"), String((age_ms + 500) / 1000));
	}
	last_value_BMX280_T.append_to(json);
	last_value_BMX280_P.append_to(json);
	last_value_BME280_H.append_to(json);
	add_Value2Json(json, F("BME280_dew_point"), dew_point_string(last_value_BMX280_T, last_value_BME280_H));
	last_value_BMX280x_T.append_to(json);
	last_value_BMX280x_P.append_to(json);
	last_value_BME280x_H.append_to(json);
	add_Value2Json(json, F("BME280x_dew_point"), dew_point_string(last_value_BMX280x_T, last_value_BME280x_H));
	last_value_SDS_P1.append_to(json);
	last_value_SDS_P2.append_to(json);
	add_Value2Json(json, F("count_measurements"), String(count_sends));
	add_Value2Json(json, F("interval"), String(cfg::sending_intervall_ms));
	add_Value2Json(json, F("signal"), String(last_signal_strength));
	json = json.substring(0, json.length() - 1) + F("]}");

	json.replace(F("[{"), F("[\n{"));
	json.replace(F("},"), F("},\n"));
	return json;
}

static void webserver_xdata_json()
{
	debug_outln_info(F("ws: xdata.json..."));
	server.send(200, FPSTR(TXT_CONTENT_TYPE_JSON), get_xdata_json());
}

/*****************************************************************
 * taylored display (lcd_2004)                                   *
 *****************************************************************/
static void append(String& line, const String& value, unsigned int minWidth)
{
	for (int spaces = minWidth - value.length(); spaces > 0; --spaces)
	{
		line += ' ';
	}
	line += value;
}

static void append(String& line, float value, float minValue, unsigned int minWidth, unsigned char decimalPlaces)
{
	String text = isnan(value) || value <= minValue ? F("-") : String(value, decimalPlaces);
	append(line, text, minWidth);
}

static void append(String& line, long count)
{
	if (count == -1)
	{
		line += F("    ? ");
		return;
	}
	else if (count < 1000)
	{
		append(line, String(count), 5);
		line += ' ';
		return;
	}
	else if (count < 1000 * 1000)
	{
		append(line, String((count + 500) / 1000), 4);
		line += 'k';
	}
	else if (count < 1000 * 1000 * 1000)
	{
		append(line, String((count + 500 * 1000) / (1000 * 1000)), 4);
		line += 'M';
	}
	else
	{
		append(line, String((count + 500 * 1000 * 1000) / (1000 * 1000 * 1000)), 4);
		line += 'G';
	}
	line += (count & 1) != 0 ? '.' : ' ';
}

static void get_remote_data();

static void display_xvalues()
{
	get_remote_data();

	#if 0 // TODO: just for display test
 	count_measurements_r = count_sends + 998;
	last_signal_strength_r = last_signal_strength;
	last_value_BMX280_T = 22;
	last_value_BMX280_P = 1000 * 100;
	last_value_BME280_H = 60;
	last_value_BMX280_T_r = -10;
	last_value_BMX280_P_r = 1001 * 100;
	last_value_BME280_H_r = 80;
	last_value_BMX280x_T_r = 0.1;
	last_value_BMX280x_P_r = 1002 * 100;
	last_value_BME280x_H_r = 100;
	last_value_SDS_P2_r = 2;
	last_value_SDS_P1_r = 7;
	#endif

	/*
		    this remote xremote
		12345678901234567890
		--------------------
		°C -10.0 -10.0 -10.0
		H % 60.0  80.0   100
		hPA 1000  1000  1000
		14°C 25/30µ 999k 30%
		--------------------
		Dew  PM of  |    WiFi of remote
		this remote |#measurements of remote
		     2.5/10µ
		     µg/m³
	*/
	float temperature[3] = { last_value_BMX280_T, last_value_BMX280_T_r, last_value_BMX280x_T_r };
	float pressure   [3] = { last_value_BMX280_P, last_value_BMX280_P_r, last_value_BMX280x_P_r };
	float humidity   [3] = { last_value_BME280_H, last_value_BME280_H_r, last_value_BME280x_H_r };

	String centiDegrees(char(223));
	centiDegrees += 'C';
	String line0 = centiDegrees;
	String line1 = F("H %");
	String line2 = F("hPa");
	String line3;
	for (int i = 0; i < 3; ++i)
	{
		append(line0, temperature[i], -128, 6, 1);
		append(line1, humidity[i], -1, i == 0 ? 5 : 6, humidity[i] >= 99.5 ? 0 : 1);
		append(line2, pressure[i] * 0.01f, -0.01f, i == 0 ? 5 : 6, 0);
	}
	append(line3, dew_point(last_value_BMX280_T, last_value_BME280_H), -128, 2, 0);
	line3 += centiDegrees;
	append(line3, last_value_SDS_P2_r, -1, 3, 0);
	line3 += '/';
	append(line3, last_value_SDS_P1_r, -1, 2, 0);
	line3 += 'u';
	append(line3, count_measurements_r);
	append(line3, String(calcWiFiSignalQuality(last_signal_strength_r)), 2);
	line3 += '%';

	if (lcd_2004)
	{
		lcd_2004->setCursor(0, 0);
		lcd_2004->print(line0);
		lcd_2004->setCursor(0, 1);
		lcd_2004->print(line1);
		lcd_2004->setCursor(0, 2);
		lcd_2004->print(line2);
		lcd_2004->setCursor(0, 3);
		lcd_2004->print(line3);
	}

	#if CFG_DEBUG_OUT_DISPLAY // output to serial, too
	debug_outln_info(line0);
	debug_outln_info(line1);
	debug_outln_info(line2);
	debug_outln_info(line3);
	#endif

	yield();
}

/*****************************************************************
 * get remote data via HTTP                                      *
 *****************************************************************/
const char* extract_value(const String& json, const char* name)
{
	const char* pos = strstr(json.begin(), name);
	if (pos == nullptr || pos >= json.end())
	{
		return nullptr;
	}
	pos += strlen(name) + strlen("\",\"value\":\"");
	if (pos >= json.end())
	{
		return nullptr;
	}
	return pos;
}

long extract_long(const String& json, const char* name, long invalid)
{
	const char* value = extract_value(json, name);
	return value != nullptr ? atol(value) : invalid;
}

float extract_float(const String& json, const char* name, float invalid)
{
	const char* value = extract_value(json, name);
	return value != nullptr ? static_cast<float>(atof(value)) : invalid;
}

void extract_data(const String& json, const time_t now)
{
	count_measurements_r   = extract_long(json, "count_measurements", -1);
	last_signal_strength_r = extract_long(json, "signal", 0);
	last_value_BMX280_T_r  = extract_float(json, "BME280_temperature", -128.);
	last_value_BMX280_P_r  = extract_float(json, "BME280_pressure", -1.);
	last_value_BME280_H_r  = extract_float(json, "BME280_humidity", -1.);
	last_value_BMX280x_T_r = extract_float(json, "BME280x_temperature", -128.);
	last_value_BMX280x_P_r = extract_float(json, "BME280x_pressure", -1.);
	last_value_BME280x_H_r = extract_float(json, "BME280x_humidity", -1.);
	last_value_SDS_P2_r    = extract_float(json, "SDS_P2", -1.);
	last_value_SDS_P1_r    = extract_float(json, "SDS_P1", -1.);

	const long age_s = extract_long(json, "age", 0);
	const long interval_ms = extract_long(json, "interval", -1);
	if (age_s < 0)
	{
		next_update_r = now - age_s + 1;
	}
	else if (interval_ms > 0)
	{
		next_update_r = now - age_s + interval_ms / 1000 + 1;
	}
	else
	{
		next_update_r = 0;
	}
}

const String remoteUri = "/xdata.json";
WiFiClient remoteWiFiClient;
HTTPClient remoteHttpClient;

static bool open_remote()
{
	if (remoteHttpClient.connected())
	{
		return true;
	}

	remoteHttpClient.setTimeout(2 * 1000);
	remoteHttpClient.setUserAgent(SOFTWARE_VERSION + '/' + esp_chipid + '/' + esp_mac_id);
	remoteHttpClient.setReuse(true);
	if (remoteHttpClient.begin(remoteWiFiClient, remoteHost, 80, remoteUri, /*https*/false))
	{
		#if CFG_DEBUG_REMOTE_CONNECTION
		debug_outln_info(F("Connection prepared to "), remoteHost);
		#endif
		return true;
	}
	else
	{
		debug_outln_info(F("Failed connecting to "), remoteHost);
		return false;
	}
}

static void get_remote_data()
{
	const time_t now = time(nullptr);
	if (now < next_update_r)
	{
		return;
	}

	String json;

	#if CFG_FAKE_REMOTE_DATA
	json = get_xdata_json();
	#else
	if (open_remote())
	{
		const int result = remoteHttpClient.GET();
		if (result == HTTP_CODE_OK)
		{
			json = remoteHttpClient.getString();
			#if CFG_DEBUG_OUT_REMOTE_DATA
			debug_outln_info(F("GET returned"));
			debug_outln_info(json);
			debug_outln_info(F("GET end"));
			#endif
		}
		else
		{
			debug_outln_info(F("GET request failed with error: "), String(result));
			if (result >= HTTP_CODE_BAD_REQUEST)
			{
				debug_outln_info(F("Details:"), remoteHttpClient.getString());
			}
			return;
		}
	}
	#endif

	extract_data(json, now);
}
