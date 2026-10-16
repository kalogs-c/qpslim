export function parsePid(text: string): number | null {
  const n = Number.parseInt(text, 10);
  return Number.isInteger(n) && n > 0 ? n : null;
}

export function stepIndex(index: number, delta: number, length: number): number {
  return (index + delta + length) % length;
}
