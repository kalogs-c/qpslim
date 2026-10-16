<script lang="ts">
  import { onDestroy, onMount } from "svelte";
  import { getCurrentWindow } from "@tauri-apps/api/window";
  import {
    PRESETS,
    getStats,
    setEnabled,
    setTarget,
    type Stats
  } from "$lib/limiter";
  import { parsePid, stepIndex } from "$lib/selection";

  let pidText = $state("");
  let index = $state(PRESETS.indexOf(60));
  let enabled = $state(false);
  let stats = $state<Stats | null>(null);
  let notice = $state("");

  const pid = $derived.by(() => parsePid(pidText));
  const target = $derived(PRESETS[index]);

  function fail(message: unknown) {
    notice = message instanceof Error ? message.message : String(message);
    console.error("[qpslim]", notice);
  }

  async function pushTarget() {
    if (enabled && pid !== null) {
      try {
        await setTarget(pid, target);
        notice = "";
      } catch (e) {
        fail(e);
      }
    }
  }

  function moveSelection(delta: number) {
    selectPreset(stepIndex(index, delta, PRESETS.length));
  }

  function selectPreset(i: number) {
    index = i;
    void pushTarget();
  }

  async function toggle() {
    if (pid === null) {
      return;
    }
    try {
      if (enabled) {
        await setEnabled(pid, false);
        enabled = false;
      } else {
        await setTarget(pid, target);
        enabled = true;
      }
      notice = "";
    } catch (e) {
      fail(e);
    }
  }

  async function refreshStats() {
    if (pid === null || document.hidden) {
      return;
    }
    try {
      stats = await getStats(pid);
      notice = "";
    } catch (e) {
      stats = null;
      fail(e);
    }
  }

  async function hideOverlay() {
    try {
      const appWindow = await getCurrentWindow();
      await appWindow.hide();
    } catch (e) {
      fail(e);
    }
  }

  async function toggleVisible() {
    try {
      const appWindow = await getCurrentWindow();
      if (await appWindow.isVisible()) {
        await appWindow.hide();
      } else {
        await appWindow.show();
      }
    } catch (e) {
      fail(e);
    }
  }

  function onKeyDown(event: KeyboardEvent) {
    const typing = event.target instanceof HTMLInputElement;
    if (typing) {
      if (event.key === "Escape") {
        (event.target as HTMLInputElement).blur();
      }
      return;
    }
    switch (event.key) {
      case "ArrowUp":
        event.preventDefault();
        void moveSelection(-1);
        break;
      case "ArrowDown":
        event.preventDefault();
        void moveSelection(1);
        break;
      case "Enter":
      case " ":
        event.preventDefault();
        void toggle();
        break;
      case "Escape":
        void hideOverlay();
        break;
      case "F11":
        event.preventDefault();
        void toggleVisible();
        break;
    }
  }

  let timer: number | undefined;
  onMount(() => {
    timer = window.setInterval(() => void refreshStats(), 1000);
  });
  onDestroy(() => {
    window.clearInterval(timer);
  });
</script>

<svelte:window onkeydown={onKeyDown} />

<main class="panel">
  <div class="pid">
    <label for="pid">pid:</label>
    <input
      id="pid"
      inputmode="numeric"
      placeholder="—"
      bind:value={pidText}
      onchange={() => void refreshStats()}
    />
  </div>

  <button class="target" class:on={enabled} onclick={() => void toggle()}>
    <span class="value">{target}</span>
    <span class="unit">FPS</span>
  </button>

  <ol class="presets">
    {#each PRESETS as fps, i}
      <li>
        <button
          class="preset"
          class:active={i === index}
          onclick={() => void selectPreset(i)}
        >
          {fps}
        </button>
      </li>
    {/each}
  </ol>

  <div class="current">{stats ? `${stats.fps_avg.toFixed(1)} fps` : "—"}</div>
  {#if notice}
    <div class="notice">{notice}</div>
  {/if}
</main>

<style>
  .panel {
    width: 100%;
    height: 100vh;
    margin: 0;
    display: flex;
    flex-direction: column;
    align-items: center;
    gap: 8px;
    padding: 12px;
    box-sizing: border-box;
    font-family: system-ui, sans-serif;
    background: #1c1c1e;
    color: #f2f2f7;
    user-select: none;
  }
  .pid {
    display: flex;
    align-items: center;
    gap: 6px;
    font-size: 12px;
    opacity: 0.7;
    align-self: flex-start;
  }
  .pid input {
    width: 80px;
    background: transparent;
    border: 1px solid #3a3a3c;
    border-radius: 4px;
    color: inherit;
    font-size: 12px;
    padding: 2px 6px;
    outline: none;
  }
  .target {
    border: 2px solid transparent;
    border-radius: 12px;
    background: transparent;
    color: inherit;
    padding: 10px 36px;
    cursor: pointer;
    display: flex;
    align-items: baseline;
    gap: 8px;
  }
  .target .value {
    font-size: 44px;
    font-weight: 700;
  }
  .target .unit {
    font-size: 14px;
    opacity: 0.7;
  }
  .target.on {
    background: #30d158;
    color: #06130a;
  }
  .target.on .unit {
    opacity: 0.8;
  }
  .presets {
    list-style: none;
    display: flex;
    gap: 4px;
    margin: 0;
    padding: 0;
  }
  .preset {
    background: transparent;
    border: 1px solid transparent;
    border-radius: 6px;
    color: inherit;
    font-size: 12px;
    padding: 2px 6px;
    cursor: pointer;
    opacity: 0.55;
  }
  .preset.active {
    opacity: 1;
    border-color: #636366;
  }
  .current {
    font-size: 12px;
    opacity: 0.45;
  }
  .notice {
    font-size: 11px;
    opacity: 0.6;
    max-width: 100%;
    text-align: center;
    white-space: normal;
    overflow-wrap: break-word;
  }
</style>
