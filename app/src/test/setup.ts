// jsdom ships without WebCrypto, which mockIPC needs internally.
// Math.random is enough here: ids only need to be unique per test.
let nextId = 1;
Object.defineProperty(window, "crypto", {
  value: {
    getRandomValues: (buffer: Uint32Array) => {
      for (let i = 0; i < buffer.length; i++) {
        buffer[i] = nextId++;
      }
      return buffer;
    }
  }
});
