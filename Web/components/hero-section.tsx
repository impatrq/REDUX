import Link from "next/link"
import Image from "next/image"
import { Button } from "@/components/ui/button"
import { HeroChevronBackground } from "@/components/hero-chevron-background"

export function HeroSection() {
  return (
    <section className="relative flex min-h-[calc(100vh-4rem)] flex-col items-center justify-center px-4 py-20 text-center">
      {/* Background decoration */}
      <div className="absolute inset-0 z-0 overflow-hidden">
        <div className="absolute -top-40 -right-40 h-80 w-80 rounded-full bg-secondary/40 blur-3xl" />
        <div className="absolute -bottom-40 -left-40 h-80 w-80 rounded-full bg-primary/30 blur-3xl" />
      </div>

      <HeroChevronBackground />
      
      <div className="relative z-10 max-w-4xl">
        
        <h1 className="mb-6 text-5xl font-bold tracking-tight sm:text-6xl lg:text-7xl">
          <span className="inline-flex items-center justify-center">
            <Image
              src="/logotipo.svg"
              alt="REDUX"
              width={508}
              height={286}
              priority
              className="h-[120px] w-auto rounded-2xl shadow-lg sm:h-[140px] lg:h-[160px]"
            />
          </span>
        </h1>

        
        <p className="mb-10 max-w-2xl mx-auto text-base text-muted-foreground/80 leading-relaxed">
          Exoesqueleto mecanico para la rehabilitacion y recuperacion de pacientes que hayan sufrido accidentes traumatologicos.
        </p>
        
        <div className="flex flex-col gap-4 sm:flex-row sm:justify-center">
          <Button asChild size="lg" className="px-8 py-6 text-lg">
            <Link href="/contacto">Contáctanos</Link>
          </Button>
          <Button
            asChild
            variant="outline"
            size="lg"
            className="border-primary text-primary hover:bg-primary hover:text-primary-foreground px-8 py-6 text-lg"
          >
            <a href="#saber-mas">Saber mas</a>
          </Button>
        </div>
      </div>
      

    </section>
  )
}
