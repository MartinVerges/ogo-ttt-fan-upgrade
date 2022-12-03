
<script>
    import { variables } from '$lib/utils/variables';
  	import { onMount } from 'svelte';
    import { Button, FormGroup, Input, Label } from 'sveltestrap';
    import Fa from 'svelte-fa/src/fa.svelte';
    import { faFloppyDisk } from '@fortawesome/pro-solid-svg-icons/faFloppyDisk';
    import { toast } from '@zerodevx/svelte-toast';

    let config = {};

    onMount(async () => {
		const response = await fetch(`/api/config`, {
            headers: { "Content-type": "application/json" }
        }).catch(error => console.log(error));
        if(response.ok) config = await response.json();
        else {
            toast.push(`Error ${response.status} ${response.statusText}<br>Unable to receive current settings.`, variables.toast.error)
        }
	});

    async function doSaveSettings () {
		fetch(`/api/config`, {
			method: 'POST',
			body: JSON.stringify(config),
            headers: { "Content-type": "application/json" }
		}).then(response => {
            if (response.ok) {
                toast.push(`Settings successfully saved`, variables.toast.success)
            } else {
                toast.push(`Error ${response.status} ${response.statusText}<br>Unable to store new AP configuration.`, variables.toast.error)
            }
        }).catch(error => console.log(error));
	}
</script>

<svelte:head>
  <title>Settings</title>
</svelte:head>

<h4>Configuration</h4>
<FormGroup>
    <Label for="hostname">Hostname</Label>
    <Input id="hostname" bind:value={config.hostname} pattern="^[a-zA-Z][a-zA-Z\d-]{1,32}[a-zA-Z\d]$" placeholder="changeme"  minlength="3" maxlength="32"/>
</FormGroup>
<FormGroup>
    <Input id="enablewifi" bind:checked={config.enablewifi} type="checkbox" label="Enable WiFi" />
    <Input id="enablesoftap" bind:checked={config.enablesoftap} type="checkbox" label="Create AP if no WiFi is available" />
</FormGroup>
<FormGroup>
    <p>Mixer Settings</p>
    <Input id="runMixerAfterMinutes" bind:value={config.runMixerAfterMinutes} placeholder="720" maxlength="32"/>
    <Label for="runMixerAfterMinutes">Run mixer every X Minutes (default 720 minutes == 12 hours)</Label>
    <Input id="noMixerBelowTempC" bind:value={config.noMixerBelowTempC} placeholder="10" min="-255" max="255" type="number"/>
    <Label for="noMixerBelowTempC">Prevent automatic mixer runs below temperature in °C (default +10°C)</Label>
</FormGroup>
<FormGroup>
    <p>Fan Speed Settings</p>
    <Input id="overrideSpeedPoti" bind:checked={config.overrideSpeedPoti} type="checkbox" label="Override potentiometer speed value" />
    <Input id="overrideSpeed" bind:value={config.overrideSpeed} placeholder="25" min="0" max="100" type="number" disabled={!config.overrideSpeedPoti} />
    <Label for="overrideSpeed">Idle Speed setting 0-100%</Label>
</FormGroup>
<FormGroup>
    <Input id="humidityThr" bind:value={config.humidityThr} placeholder="75" min="0" max="100" type="number" />
    <Label for="humidityThr">Speed up the fan if the humidity is equal or above this value. Set 0 to disable.</Label>
    <Input id="humiditySpeed" bind:value={config.humiditySpeed} placeholder="80" min="0" max="100" type="number" />
    <Label for="humiditySpeed">Dehumidification Speed setting 0-100%</Label>
</FormGroup>
<FormGroup> 
    <Label for="otapassword">OTA (Over The Air) firmware update password</Label>   
    <Input id="otapassword" bind:value={config.otapassword} placeholder="OTA Password" maxlength="32" />
</FormGroup>
<FormGroup>
    <p>MQTT Settings</p>
    <Input id="enablemqtt" bind:checked={config.enablemqtt} type="checkbox" label="Publish to MQTT Broker" />
    <Label for="mqtthost">MQTT Host or IP</Label>
    <Input id="mqtthost" bind:value={config.mqtthost} placeholder="mqtt.net.local" maxlength="32"/>
    <Label for="mqttport">MQTT Port (default 1883)</Label>
    <Input id="mqttport" bind:value={config.mqttport} placeholder="1883" min="1" max="65535" type="number"/>
    <Label for="mqtttopic">MQTT Topic</Label>
    <Input id="mqtttopic" bind:value={config.mqtttopic} placeholder="some/sensor" maxlength="32"/>
    <Label for="mqttuser">MQTT Username</Label>
    <Input id="mqttuser" bind:value={config.mqttuser} placeholder="Username" maxlength="32"/>
    <Label for="mqttpass">MQTT Password</Label>
    <Input id="mqttpass" bind:value={config.mqttpass} placeholder="Password" maxlength="32"/>
</FormGroup>
<Button on:click={doSaveSettings} block style="height: 5rem;"><Fa icon={faFloppyDisk} />&nbsp;Save Settings</Button>
