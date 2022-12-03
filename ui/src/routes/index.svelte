
<script>
  import { variables } from '$lib/utils/variables';
  import { Button } from 'sveltestrap';
  import { toast } from '@zerodevx/svelte-toast';
  import { onMount } from 'svelte';

  import Fa from 'svelte-fa/src/fa.svelte';
  import { faEngine } from '@fortawesome/pro-solid-svg-icons/faEngine';
  import { faDropletPercent } from '@fortawesome/pro-solid-svg-icons/faDropletPercent';
  import { faGroupArrowsRotate } from '@fortawesome/pro-solid-svg-icons/faGroupArrowsRotate';

  let status = {
    lastMixer: 0,
    stateMixer: false,
    stateDplus: false,
    statePoti: 0,
    stateFanRpm: 0,
    statePwmSpeed: 0,
    stateTemperature: 0,
    stateHumidity: 0,
  }

  // ******* START MIXER ******** //
  async function runMixer() {
    status.stateMixer = true;
    const response = await fetch(`/api/mixer/start`, {
      method: 'POST',
      headers: { "Content-type": "application/json" }
    }).catch(error => console.log(error));
    if(!response.ok) {
      toast.push(`Error ${response.status} ${response.statusText}<br>Unable to start the toilet mixer.`, variables.toast.error)
    }
  }

  onMount(async () => {
    // dynamic refreshing level
    if (!!window.EventSource) {
      var source = new EventSource('/api/events');

      source.addEventListener('error', function(e) {
        if (e.target.readyState != EventSource.OPEN) {
          console.log("Events Disconnected");
        }
      }, false);

      source.addEventListener('message', function(e) {
        console.log("message", e.data);
      }, false);

      source.addEventListener('status', function(e) {
        try {
          status = JSON.parse(e.data);
        } catch (error) {
          console.log(error);
          console.log("Error parsing status", e.data);
        }
      }, false);
    }
  })

  function convertMsToTime(ms) {
    let seconds = (ms / 1000).toFixed(1);
    let minutes = (ms / (1000 * 60)).toFixed(1);
    let hours = (ms / (1000 * 60 * 60)).toFixed(1);
    let days = (ms / (1000 * 60 * 60 * 24)).toFixed(1);

    if (seconds < 60) return seconds + " Sec";
    else if (minutes < 60) return minutes + " Min";
    else if (hours < 24) return hours + " Hrs";
    else return days + " Days"
  }
</script>

<svelte:head>
  <title>Toilet Status</title>
</svelte:head>

<div class="row row-cols-1 row-cols-md-3 mb-3 text-center">
  <div class="col">
    <div class="card h-100 mb-4 rounded-3 shadow-sm border-secondary">
      <div class="card-header py-3 text-bg-secondary border-secondary">
        <h4 class="my-0 fw-normal">Toilet Mixer {#if status.stateMixer}<Fa icon={faGroupArrowsRotate} spin />{/if}</h4>
      </div>
      <div class="card-body">
        <p>Last run:</p>
        <h2 class="card-title pricing-card-title">{convertMsToTime(status.lastMixer)} <small class="text-muted fw-light">ago</small></h2>
        <br>
        <Button on:click={()=>runMixer()} block style="height: 5rem;">Start the Mixer</Button>
      </div>
    </div>
  </div>

  <div class="col">
    <div class="card h-100 mb-4 rounded-3 shadow-sm border-secondary">
      <div class="card-header py-3 text-bg-secondary border-secondary">
        <h4 class="my-0 fw-normal">Inside Environment</h4>
      </div>
      <div class="card-body">
        <p>Temperature:</p>
        <h2 class="card-title pricing-card-title">{status.stateTemperature.toFixed(1)}°C</h2>
        <p>Humidity:</p>
        <h2 class="card-title pricing-card-title">{status.stateHumidity.toFixed(1)}%</h2>
      </div>
    </div>
  </div>

  <div class="col">
    <div class="card h-100 mb-4 rounded-3 shadow-sm border-secondary">
      <div class="card-header py-3 text-bg-secondary border-secondary">
        <h4 class="my-0 fw-normal">Ventilation</h4>
      </div>
      <div class="card-body">
        <p>Current RPM:</p>
        <h2 class="card-title pricing-card-title">{Math.round(status.stateFanRpm)}</h2>
        <p>Requested Speed:</p>
        <h2 class="card-title pricing-card-title">{status.statePwmSpeed}%
          {#if status.stateMixer}
            <Fa icon={faGroupArrowsRotate} spin />
          {:else if status.stateDplus}
            <Fa icon={faEngine} />
          {:else if status.stateDehumidification}
            <Fa icon={faDropletPercent} />
          {/if}
        </h2>
      </div>
    </div>
  </div>
</div>
