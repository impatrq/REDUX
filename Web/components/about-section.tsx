import { Card, CardContent, CardHeader, CardTitle } from "@/components/ui/card"
import { Badge } from "@/components/ui/badge"

export function AboutSection() {
  return (
    <section id="saber-mas" className="scroll-mt-20 bg-primary py-20 px-4">
      <div className="container mx-auto max-w-6xl">
        <div className="text-center mb-16">
          <h2 className="text-4xl font-bold text-primary-foreground mb-4 text-balance">
            Conoce mas sobre REDUX
          </h2>
          <p className="text-primary-foreground/70 max-w-2xl mx-auto">
           ¿Que es REDUX?
          </p>
        </div>

        <div className="bg-background rounded-2xl p-8 lg:p-12">
          <div className="grid gap-8 lg:grid-cols-2 items-center">
            <div>
              <h3 className="text-3xl font-bold text-foreground mb-4">
               REDUX
              </h3>
              <p className="text-muted-foreground mb-4">
                REDUX es un exoesqueleto mecanico funcional para el brazo que ayuda a la recuperacion de los pacientes que hayan sufrido lesiones traumatologicas en los musculos de bicep y tricep 
              </p>
              <p className="text-muted-foreground">
                Meditante un motor, ayuda a los pacientes a realizar los ejercicios corresponditentes al tratamiento de recuperacion conn supervision del kinesiologo a cargo
              </p>
            </div>
            <div className="bg-secondary rounded-xl p-8">
              <h4 className="text-xl font-semibold text-secondary-foreground mb-6">Por que REDUX?</h4>
              <ul className="space-y-4">
                <li className="flex items-start gap-3">
                  <span className="flex h-6 w-6 shrink-0 items-center justify-center rounded-full bg-primary text-primary-foreground text-sm font-bold">1</span>
                  <span className="text-secondary-foreground">Eficacia</span>
                </li>
                <li className="flex items-start gap-3">
                  <span className="flex h-6 w-6 shrink-0 items-center justify-center rounded-full bg-primary text-primary-foreground text-sm font-bold">2</span>
                  <span className="text-secondary-foreground">Progreso</span>
                </li>
                <li className="flex items-start gap-3">
                  <span className="flex h-6 w-6 shrink-0 items-center justify-center rounded-full bg-primary text-primary-foreground text-sm font-bold">3</span>
                  <span className="text-secondary-foreground">Simplicidad</span>
                </li>
                <li className="flex items-start gap-3">
                  <span className="flex h-6 w-6 shrink-0 items-center justify-center rounded-full bg-primary text-primary-foreground text-sm font-bold">4</span>
                  <span className="text-secondary-foreground">Seguridad</span>
                </li>
              </ul>
            </div>
          </div>
        </div>
      </div>
    </section>
  )
}
