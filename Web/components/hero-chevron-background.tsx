const ROW_COUNT = 44

/**
 * Filas de /flechas.svg detrás del hero. Cada fila tiene la altura de un mosaico
 * completo (ancho × ratio del viewBox) para no recortar; el hueco entre filas
 * es el `gap`. Animación vertical en bucle; filas alternan subida / bajada.
 */
export function HeroChevronBackground() {
  return (
    <div
      className="pointer-events-none absolute inset-0 z-[1] overflow-hidden"
      aria-hidden
    >
      <div className="absolute left-1/2 top-1/2 h-[220vmin] w-[220vmin] -translate-x-1/2 -translate-y-1/2 -rotate-[38deg]">
        <div
          className="hero-chevron-vars flex h-full w-full flex-col justify-center opacity-[0.14] dark:opacity-[0.08]"
          style={{
            gap: "clamp(12px, 2.4vw, 26px)",
          }}
        >
          {Array.from({ length: ROW_COUNT }, (_, i) => (
            <div key={i} className="hero-chevron-row">
              <div
                className={`hero-chevron-track ${
                  i % 2 === 0
                    ? "hero-chevron-drift-up"
                    : "hero-chevron-drift-down"
                }`}
              >
                <div className="hero-chevron-strip" />
                <div className="hero-chevron-strip" />
              </div>
            </div>
          ))}
        </div>
      </div>
    </div>
  )
}
