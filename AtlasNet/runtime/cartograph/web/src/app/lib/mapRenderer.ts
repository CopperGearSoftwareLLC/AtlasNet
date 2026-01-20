// /src/lib/mapRenderer.ts

import type { ShapeJS } from './types'; // optional type definition

interface DrawOptions {
    container: HTMLDivElement; // the element to attach the canvas
    shapes: ShapeJS[];         // array of shapes (from SWIG)
}

export function createMapRenderer({ container, shapes = [] }: DrawOptions) {
    const canvas = document.createElement('canvas');
    const ctx = canvas.getContext('2d')!;
    canvas.style.touchAction = 'none'; // for drag scrolling
    container.appendChild(canvas);

    // Track pan/drag
    let offsetX = 0;
    let offsetY = 0;
    let isDragging = false;
    let lastX = 0;
    let lastY = 0;

    // Track zoom
    let scale = 1;
    const minScale = 0.2;
    const maxScale = 5;

    // Handle pointer events
    canvas.addEventListener('pointerdown', (e) => {
        isDragging = true;
        lastX = e.clientX;
        lastY = e.clientY;
        canvas.setPointerCapture(e.pointerId);
    });

    canvas.addEventListener('pointermove', (e) => {
        if (!isDragging) return;
        offsetX += e.clientX - lastX;
        offsetY -= e.clientY - lastY;
        lastX = e.clientX;
        lastY = e.clientY;
        draw();
    });

    canvas.addEventListener('pointerup', (e) => {
        isDragging = false;
        canvas.releasePointerCapture(e.pointerId);
    });

    // Zoom
    canvas.addEventListener('wheel', (e) => {
        e.preventDefault();
        const zoomFactor = 1.1;
        const mouseX = e.offsetX;
        const mouseY = e.offsetY;

        const newScale = e.deltaY < 0 ? scale * zoomFactor : scale / zoomFactor;
        if (newScale < minScale || newScale > maxScale) return;

        offsetX = mouseX - ((mouseX - offsetX) * (newScale / scale));
        offsetY = mouseY - ((mouseY - offsetY) * (newScale / scale));

        scale = newScale;
        draw();
    }, { passive: false });

    // Resize canvas on container change
    function resizeCanvas() {
        canvas.width = container.clientWidth;
        canvas.height = container.clientHeight;
        draw();
    }

    window.addEventListener('resize', resizeCanvas);
    resizeCanvas(); // initial size

    function drawAxes() {
        const minPixelSpacing = 50;
        const fontSize = 12;
        ctx.save();

        ctx.strokeStyle = '#555';
        ctx.fillStyle = '#fff';
        ctx.lineWidth = 1;
        ctx.font = `${fontSize}px sans-serif`;

        const rawStep = minPixelSpacing / scale;
        const magnitude = Math.pow(10, Math.floor(Math.log10(rawStep)));
        let niceStep = magnitude;
        if (rawStep / magnitude >= 5) niceStep = 5 * magnitude;
        else if (rawStep / magnitude >= 2) niceStep = 2 * magnitude;

        const startX = -offsetX / scale;
        const endX = (canvas.width - offsetX) / scale;
        const startY = -offsetY / scale;
        const endY = (canvas.height - offsetY) / scale;

        // Vertical lines
        // Vertical lines (x-axis labels)
        ctx.textAlign = 'center';
        ctx.textBaseline = 'bottom';
        for (let x = Math.floor(startX / niceStep) * niceStep; x <= endX; x += niceStep) {
            const screenX = x * scale + offsetX;

            // Draw line
            ctx.beginPath();
            ctx.moveTo(screenX, 0);
            ctx.lineTo(screenX, canvas.height);
            ctx.stroke();

            // Draw label (flip text back)
            ctx.save();
            ctx.scale(1, -1);           // flip vertically
            ctx.fillText(x.toFixed(0), screenX, -2); // negate y because canvas is flipped
            ctx.restore();
        }


        // Horizontal lines (y-axis labels)
        ctx.textAlign = 'right';
        ctx.textBaseline = 'middle';
        for (let y = Math.floor(startY / niceStep) * niceStep; y <= endY; y += niceStep) {
            const screenY = y * scale + offsetY;

            // Draw line
            ctx.beginPath();
            ctx.moveTo(0, screenY);
            ctx.lineTo(canvas.width, screenY);
            ctx.stroke();

            // Draw label (flip text back)
            ctx.save();
            ctx.scale(1, -1);               // flip vertically
            ctx.fillText(y.toFixed(0), canvas.width - 2, -screenY);
            ctx.restore();
        }


        ctx.restore();
    }

    function drawShapes() {
        if (!shapes || shapes.length === 0) return;

        shapes.forEach(shape => {
            const x = ((shape.position.x ?? 0) * scale) + offsetX;
            const y = ((shape.position.y ?? 0) * scale) + offsetY;

            ctx.save();
            ctx.translate(x, y);
            ctx.scale(scale, scale);

            switch (shape.type) {
                case 'circle':
                    ctx.beginPath();
                    ctx.arc(0, 0, shape.radius ?? 10, 0, Math.PI * 2);
                    ctx.strokeStyle = 'rgba(100, 149, 255, 1)';
                    //ctx.fill();
                    ctx.stroke();
                    break;
                case 'rectangle':
                    ctx.strokeStyle = 'rgba(255, 149, 100, 1)';
                    //ctx.fillRect(-(shape.size?.x ?? 10)/2, -(shape.size?.y ?? 10)/2, shape.size?.x ?? 10, shape.size?.y ?? 10);
                    ctx.strokeRect(-(shape.size?.x ?? 10) / 2, -(shape.size?.y ?? 10) / 2, shape.size?.x ?? 10, shape.size?.y ?? 10);
                    break;
                case 'polygon':
                    const pts = shape.points ?? [];
                    if (pts.length > 0) {
                        ctx.beginPath();
                        ctx.moveTo(pts[0].x, pts[0].y);
                        for (let i = 1; i < pts.length; i++) ctx.lineTo(pts[i].x, pts[i].y);
                        ctx.closePath();
                        ctx.strokeStyle = 'rgba(100, 255, 149, 1)';
                        //ctx.fill();
                        ctx.stroke();
                    }
                    break;
            }
            ctx.restore();
        });
    }

    function draw() {
        ctx.clearRect(0, 0, canvas.width, canvas.height);

        ctx.save();
        // Flip y axis
        ctx.translate(0, canvas.height); // move origin to bottom-left
        ctx.scale(1, -1); // flip y axis

        drawAxes();   // draw axes (they also need to account for flipped y)
        drawShapes(); // draw shapes

        ctx.restore();
    }


    draw();

    return {
        draw,
        setShapes(newShapes: ShapeJS[]) {
            shapes = newShapes;
            draw();
        },
        setOffset(newOffsetX: number, newOffsetY: number) {
            offsetX = newOffsetX;
            offsetY = newOffsetY;
            draw();
        },
        setScale(newScale: number) {
            scale = Math.min(maxScale, Math.max(minScale, newScale));
            draw();
        }
    };
}
