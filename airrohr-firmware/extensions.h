static float dew_point(const float temperature, const float humidity);

static String dew_point_string(const float temperature, const float humidity)
{
	const float dew_point_temp = dew_point(temperature, humidity);
	return isnan(dew_point_temp) ? "-" : String(dew_point_temp, 1);
}

float last_value_BMX280x_T = -128.0;
float last_value_BMX280x_P = -1.0;
float last_value_BME280x_H = -1.0;

const String JSON_SENSOR_DATA_VALUES_TAG = "\"sensordatavalues\":";
const String JSON_INSERT_XDATA_BEFORE_ENTRY = "{\"value_type\":\"samples\"";

/*****************************************************************
 * Webserver xdata.json                                          *
 *****************************************************************/
static void webserver_xdata_json()
{
	debug_outln_info(F("ws: xdata.json..."));

	RESERVE_STRING(json, XLARGE_STR);

	{
		long age_ms = msSince(starttime);
		if (!count_sends)
		{
			json = String(FPSTR(data_first_part)) + F("]}");
			age_ms -= cfg::sending_intervall_ms;
		}
		else
		{
			json = last_data_string;
		}
		String age = String(F("\n\"age\":\"")) + String((age_ms + 500) / 1000) + F("\", ");
		json.replace(JSON_SENSOR_DATA_VALUES_TAG, age + JSON_SENSOR_DATA_VALUES_TAG);
	}

	{
		RESERVE_STRING(xdata, LARGE_STR);
		add_Value2Json(xdata, F("BME280_dew_point"), dew_point_string(last_value_BMX280_T, last_value_BME280_H));
		add_Value2Json(xdata, F("BME280x_temperature"), FPSTR(DBG_TXT_TEMPERATURE), last_value_BMX280x_T);
		add_Value2Json(xdata, F("BME280x_pressure"), FPSTR(DBG_TXT_PRESSURE), last_value_BMX280x_P);
		add_Value2Json(xdata, F("BME280x_humidity"), FPSTR(DBG_TXT_HUMIDITY), last_value_BME280x_H);
		add_Value2Json(xdata, F("BME280x_dew_point"), dew_point_string(last_value_BMX280x_T, last_value_BME280x_H));
		json.replace(JSON_INSERT_XDATA_BEFORE_ENTRY, xdata + JSON_INSERT_XDATA_BEFORE_ENTRY);
	}

	json.replace(F("[{"), F("[\n{"));
	json.replace(F("},"), F("},\n"));
	server.send(200, FPSTR(TXT_CONTENT_TYPE_JSON), json);
}
