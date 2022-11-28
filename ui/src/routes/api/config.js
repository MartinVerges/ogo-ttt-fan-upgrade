
export async function get({request}) {
    return {
        status: 200,
        body: {
            "enablemqtt": true,
            "enablesoftap": true,
            "enablewifi": true,
            "otapassword": "abcd1234567890",
            "hostname": "toilet",
            "mqtthost": "192.168.255.1",
            "mqttpass": "abcd1234",
            "mqttport": 1883,
            "mqtttopic": "toilet",
            "mqttuser": "toilet",
            "otaPassword": "0123456789"
        }
    }
}

export async function post({request}) {
    return {
        status: 200,
        body: {
            "message": "success"
        }
    }
}