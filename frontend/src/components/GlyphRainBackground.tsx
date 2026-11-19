import { useEffect, useRef } from 'react';

const GLYPHS = 'ｱｲｳｴｵｶｷｸｹｺｻｼｽｾｿﾀﾁﾂﾃﾄﾅﾆﾇﾈﾉﾊﾋﾌﾍﾎﾏﾐﾑﾒﾓﾔﾕﾖﾗﾘﾙﾚﾛﾜ0123456789ABCDEF';

type Stream = {
  x: number;
  y: number;
  speed: number;
  size: number;
  trail: number;
  glyphs: string[];
  color: string;
  nextShuffle: number;
};

function randomGlyph() {
  return GLYPHS[Math.floor(Math.random() * GLYPHS.length)];
}

function readColor(element: HTMLElement, property: string, fallback: string) {
  return getComputedStyle(element).getPropertyValue(property).trim() || fallback;
}

function alphaColor(color: string, alpha: number) {
  const value = color.replace('#', '');
  const hex = value.length === 3 ? value.split('').map((part) => part + part).join('') : value;
  const red = Number.parseInt(hex.slice(0, 2), 16);
  const green = Number.parseInt(hex.slice(2, 4), 16);
  const blue = Number.parseInt(hex.slice(4, 6), 16);
  return `rgba(${red}, ${green}, ${blue}, ${alpha})`;
}

function makeStreams(width: number, height: number, trailColor: string, headColor: string): Stream[] {
  const streams: Stream[] = [];
  const layers = [
    { columnWidth: 38, size: 11, speed: 22, trail: 9, alpha: 0.52 },
    { columnWidth: 27, size: 14, speed: 38, trail: 13, alpha: 0.76 },
  ];

  layers.forEach((layer, layerIndex) => {
    const columns = Math.ceil(width / layer.columnWidth);
    for (let column = 0; column < columns; column += 1) {
      if (Math.random() > (layerIndex === 0 ? 0.46 : 0.56)) continue;
      streams.push({
        x: column * layer.columnWidth + Math.random() * 10,
        y: Math.random() * height,
        speed: layer.speed * (0.65 + Math.random() * 0.9),
        size: layer.size,
        trail: layer.trail + Math.floor(Math.random() * 5),
        glyphs: Array.from({ length: layer.trail + 5 }, randomGlyph),
        color: Math.random() > 0.28 ? trailColor : headColor,
        nextShuffle: 0,
      });
    }
  });

  return streams;
}

export default function GlyphRainBackground() {
  const canvasRef = useRef<HTMLCanvasElement>(null);

  useEffect(() => {
    const canvas = canvasRef.current;
    const page = canvas?.parentElement;
    const context = canvas?.getContext('2d');
    if (!canvas || !page || !context) return;

    const reducedMotion = window.matchMedia('(prefers-reduced-motion: reduce)');
    const colorScheme = window.matchMedia('(prefers-color-scheme: light)');
    let width = 0;
    let height = 0;
    let pixelRatio = 1;
    let streams: Stream[] = [];
    let frame = 0;
    let previousTime = 0;
    let trailColor = '#0091ff';
    let headColor = '#00d2ff';

    const resize = () => {
      const bounds = page.getBoundingClientRect();
      width = bounds.width;
      height = bounds.height;
      pixelRatio = Math.min(1.5, window.devicePixelRatio || 1);
      canvas.width = Math.max(1, Math.round(width * pixelRatio));
      canvas.height = Math.max(1, Math.round(height * pixelRatio));
      context.setTransform(pixelRatio, 0, 0, pixelRatio, 0, 0);
      trailColor = readColor(page, '--auth-accent-secondary', colorScheme.matches ? '#0082b4' : '#0091ff');
      headColor = readColor(page, '--auth-accent', colorScheme.matches ? '#0082b4' : '#00d2ff');
      streams = makeStreams(width, height, trailColor, headColor);
      previousTime = 0;
      draw(0, false);
    };

    const draw = (time: number, animate: boolean) => {
      context.clearRect(0, 0, width, height);
      context.textAlign = 'center';
      context.textBaseline = 'middle';
      context.shadowBlur = 0;

      streams.forEach((stream) => {
        if (animate && time >= stream.nextShuffle) {
          const index = Math.floor(Math.random() * stream.trail);
          stream.glyphs[index] = randomGlyph();
          stream.nextShuffle = time + 80 + Math.random() * 240;
        }

        for (let index = stream.trail - 1; index >= 0; index -= 1) {
          const fade = (1 - index / stream.trail) ** 1.7;
          const isHead = index === 0;
          const alpha = fade * (isHead ? 0.95 : 0.28);
          context.font = `${stream.size}px ui-monospace, SFMono-Regular, Menlo, monospace`;
          context.fillStyle = alphaColor(isHead ? headColor : stream.color, alpha);
          if (isHead) {
            context.shadowColor = alphaColor(headColor, 0.72);
            context.shadowBlur = 8;
          } else {
            context.shadowBlur = 0;
          }
          context.fillText(stream.glyphs[index], stream.x, stream.y - index * stream.size * 1.15);
        }
      });

      context.shadowBlur = 0;
      if (animate) {
        const delta = previousTime ? Math.min((time - previousTime) / 1000, 0.05) : 0;
        previousTime = time;
        streams.forEach((stream) => {
          stream.y += stream.speed * delta;
          if (stream.y - stream.trail * stream.size > height) {
            stream.y = -Math.random() * height * 0.25;
            stream.x = Math.random() * width;
            stream.glyphs = Array.from({ length: stream.trail + 5 }, randomGlyph);
          }
        });
        frame = window.requestAnimationFrame(animateFrame);
      }
    };

    const animateFrame = (time: number) => draw(time, true);
    const start = () => {
      window.cancelAnimationFrame(frame);
      frame = 0;
      if (reducedMotion.matches) {
        draw(0, false);
      } else {
        frame = window.requestAnimationFrame(animateFrame);
      }
    };

    const observer = new ResizeObserver(resize);
    observer.observe(page);
    resize();
    start();
    reducedMotion.addEventListener('change', start);
    colorScheme.addEventListener('change', resize);

    return () => {
      window.cancelAnimationFrame(frame);
      observer.disconnect();
      reducedMotion.removeEventListener('change', start);
      colorScheme.removeEventListener('change', resize);
    };
  }, []);

  return <canvas ref={canvasRef} className="glyph-rain" aria-hidden="true" />;
}
