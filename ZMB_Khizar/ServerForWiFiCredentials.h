// #include <WebServer.h>
#include <ESPAsyncWebServer.h>

AsyncWebServer server(80);

// Create an instance of the web server on port 80
// WebServer server(80);

String ssid;
String password;
String uid;
String api;
int8_t wifi_channel = 0;

//////////////////HMI Config HTML/////////////////////////////////
const char wifimanager[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<title>BITA Zonemaster</title>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    html {
    font-family: Arial, Helvetica, sans-serif; 
    display: inline-block; 
    text-align: center;
    }

    h1 {
      font-size: 1.8rem; 
      color: white;
    }

    p { 
      font-size: 1.4rem;
    }

    .topnav { 
      overflow: hidden; 
      background-color: #0A1128;
    }

    body {  
      margin: 0;
    }

    .content { 
      padding: 5%;
    }

    .card-grid { 
      max-width: 800px; 
      margin: 0 auto; 
      display: grid; 
      grid-gap: 2rem; 
      grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
    }

    .card { 
      background-color: white; 
      box-shadow: 2px 2px 12px 1px rgba(140,140,140,.5);
    }

    .card-title { 
      font-size: 1.2rem;
      font-weight: bold;
      color: #034078
    }

    input[type=submit] {
      border: none;
      color: #FEFCFB;
      background-color: #034078;
      padding: 15px 15px;
      text-align: center;
      text-decoration: none;
      display: inline-block;
      font-size: 16px;
      width: 100px;
      margin-right: 10px;
      border-radius: 4px;
      transition-duration: 0.4s;
      }

    input[type=submit]:hover {
      background-color: #1282A2;
    }

    input[type=text], input[type=number], select {
      width: 50%;
      padding: 12px 20px;
      margin: 18px;
      display: inline-block;
      border: 1px solid #ccc;
      border-radius: 4px;
      box-sizing: border-box;
    }

    label {
      font-size: 1.2rem; 
    }
    .value{
      font-size: 1.2rem;
      color: #1282A2;  
    }
    .state {
      font-size: 1.2rem;
      color: #1282A2;
    }
    button {
      border: none;
      color: #FEFCFB;
      padding: 15px 32px;
      text-align: center;
      font-size: 16px;
      width: 100px;
      border-radius: 4px;
      transition-duration: 0.4s;
    }
    .button-on {
      background-color: #034078;
    }
    .button-on:hover {
      background-color: #1282A2;
    }
    .button-off {
      background-color: #858585;
    }
    .button-off:hover {
      background-color: #252524;
    } 
    * {
      box-sizing: border-box;
    }
    .column {
      float: left;
      width: 50%;
      padding: 10px;
    }

    /* Clear floats after the columns */
    .row:after {
      content: "";
      display: table;
      clear: both;
    }
  </style>
</head>
<body>
  <div class="topnav">
    <h1>BITA RMS</h1>
  </div>
  <div class="content">
    <div class="card-grid">
      <div class="card">
        <form action="/wifi_param" method="POST">
          <p>
            <label for="ssid">SSID</label>
            <input type="text" id ="ssid" name="ssid"><br>
            <label for="pass">Password</label>
            <input type="text" id ="pass" name="pass"><br>
            <input type ="submit" value ="Submit">
          </p>
        </form>
      </div>
    </div>
  </div>
</body>
</html>)rawliteral";

////////////////////////Wifi Manager////////////////////////////

const char *PARAM_INPUT_1 = "ssid";
const char *PARAM_INPUT_2 = "pass";

void Access_Point() {

  WiFi.mode(WIFI_AP);
  WiFi.softAP(devicename.c_str(), "bitahomes", 0, 1);  // Set your desired SSID and password
  Serial.println("AP Lauched with ssid and password:");
  Serial.println(devicename);
  Serial.println("bitahomes");

  IPAddress IP = WiFi.softAPIP();

#ifdef DEBUG
  Serial.print("AP IP address: ");
  Serial.println(IP);
#endif
}

bool connect_to_wifi() {

  WiFi.mode(WIFI_MODE_APSTA);
  WiFi.setHostname(hostname.c_str());

  WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE);

  unsigned long start_time = millis();

  WiFi.begin(ssid.c_str(), password.c_str());

#ifdef DEBUG
  Serial.println("Saved settings:");
  Serial.println("SSID: " + ssid);
  Serial.println("Password: " + password);
  Serial.print("Connecting to WiFi..");
#endif

  while (WiFi.status() != WL_CONNECTED) {

    if (millis() - start_time >= 10000) {
      // Timeout reached, stop trying to connect
#ifdef DEBUG
      Serial.println();
      Serial.println("Failed to connect to WiFi within 10 Seconds.");
#endif
      Access_Point();
      return false;  // Exit the function or handle the failure case
    }
    delay(50);

#ifdef DEBUG
    Serial.print(".");
#endif
  }

  myIP = WiFi.localIP().toString();

#ifdef DEBUG
  Serial.println();
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("connected with IP: ");
  Serial.println(myIP);
#endif

  wifi_channel = WiFi.channel();
  WiFi.softAP(devicename.c_str(), "bitahomes", wifi_channel, 1);

#ifdef DEBUG
  Serial.println("AP Lauched with ssid and password:");
  Serial.println(devicename);
  Serial.println("bitahomes");
#endif
  wifi_ap_mode = true;
  wifi_setting_time = millis();
  return true;
}

void setup_wifi_credentials() {

  preferences.begin("wifi_params", false);
  ssid = preferences.getString("ssid", "BITA DEV");
  password = preferences.getString("password", "xttok2fb");
  preferences.end();

#ifdef DEBUG
  Serial.print("Devicename: ");
  Serial.println(devicename);
  Serial.print("ssid: ");
  Serial.println(ssid);
  Serial.print("password: ");
  Serial.println(password);
#endif

  if (ssid.length() > 0 && password.length() > 0) {
    connect_to_wifi();
#ifdef DEBUG
    Serial.println("Credentials Found");
#endif
  }  //
  else {
#ifdef DEBUG
    Serial.println("Credentials Not Found");
    Serial.println("AP Mode");
#endif
    Access_Point();
  }

#ifdef DEBUG
  Serial.println("Server Begin");
#endif
}