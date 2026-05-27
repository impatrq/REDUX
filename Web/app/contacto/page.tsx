import Link from "next/link"
import { Header } from "@/components/header"
import { Footer } from "@/components/footer"
import { Mail, Instagram, ArrowRight } from "lucide-react"

export default function ContactoPage() {
  return (
    <div className="min-h-screen flex flex-col bg-background text-foreground">
      <Header />

      <main className="flex-1">
        <section className="relative overflow-hidden border-b border-border">
          <div className="absolute inset-0 bg-[linear-gradient(90deg,var(--brand-sky),transparent_45%),linear-gradient(135deg,transparent_0%,transparent_55%,color-mix(in_srgb,var(--brand-blue)_30%,transparent)_55%,color-mix(in_srgb,var(--brand-blue)_30%,transparent)_100%)]" />
          <div className="absolute -right-24 -top-24 h-80 w-80 rounded-full bg-primary/10 blur-3xl" />
          <div className="absolute -left-24 -bottom-24 h-80 w-80 rounded-full bg-primary/10 blur-3xl" />

          <div className="relative container mx-auto max-w-6xl px-4 py-14 md:py-16">
            <div className="max-w-2xl">
              <p className="text-sm font-semibold tracking-wide text-primary">
                CONTACTO
              </p>
              <h1 className="mt-2 text-3xl font-bold text-foreground md:text-4xl">
                Conocé nuestros canales de contacto
              </h1>
              <p className="mt-3 text-muted-foreground">
                Si lo deseas, podés escribirnos por email o contactarnos por Instagram.
              </p>
              <div className="mt-6 flex flex-wrap gap-3">
                <Link
                  href="mailto:redux26.project@gmail.com"
                  className="inline-flex items-center gap-2 rounded-lg bg-primary px-4 py-2 text-sm font-medium text-primary-foreground hover:bg-primary/90"
                >
                  Enviar email <ArrowRight className="h-4 w-4" />
                </Link>
                <Link
                  href="/#saber-mas"
                  className="inline-flex items-center gap-2 rounded-lg border border-border bg-background/70 px-4 py-2 text-sm font-medium text-foreground hover:bg-background"
                >
                  Saber más <ArrowRight className="h-4 w-4" />
                </Link>
              </div>
            </div>
          </div>
        </section>

        <section className="container mx-auto max-w-6xl px-4 py-10 md:py-12">
          <h2 className="text-lg font-semibold text-foreground">
            Opciones para que nos contactes fácilmente
          </h2>

          <div className="mt-6 grid gap-4 md:grid-cols-2">
            <div className="rounded-xl border border-border bg-card p-5 shadow-sm">
              <div className="flex items-start gap-4">
                <div className="flex h-11 w-11 items-center justify-center rounded-xl bg-secondary text-secondary-foreground">
                  <Mail className="h-5 w-5" />
                </div>
                <div className="min-w-0">
                  <p className="font-semibold text-foreground">Email</p>
                  <p className="mt-1 text-sm text-muted-foreground">
                    Escribinos a{" "}
                    <Link
                      href="mailto:redux26.project@gmail.com"
                      className="font-medium text-primary underline-offset-4 hover:underline"
                    >
                      redux26.project@gmail.com
                    </Link>
                    .
                  </p>
                </div>
              </div>
            </div>

            <div className="rounded-xl border border-border bg-card p-5 shadow-sm">
              <div className="flex items-start gap-4">
                <div className="flex h-11 w-11 items-center justify-center rounded-xl bg-secondary text-secondary-foreground">
                  <Instagram className="h-5 w-5" />
                </div>
                <div className="min-w-0">
                  <p className="font-semibold text-foreground">Instagram</p>
                  <p className="mt-1 text-sm text-muted-foreground">
                    También podés contactarnos por{" "}
                    <Link
                      href="https://www.instagram.com/proyecto.redux?igsh=MTdlenllcmVibGJncQ=="
                      target="_blank"
                      rel="noreferrer"
                      className="font-medium text-primary underline-offset-4 hover:underline"
                    >
                      @proyecto.redux
                    </Link>
                    .
                  </p>
                </div>
              </div>
            </div>
          </div>

          <div className="mt-10 rounded-xl border border-border bg-background p-6">
            <h3 className="text-base font-semibold text-foreground">
              ¿Cuál canal te conviene?
            </h3>
            <p className="mt-2 text-sm text-muted-foreground">
              - Para mensajes formales o adjuntar información:{" "}
              <Link
                href="mailto:redux26.project@gmail.com"
                className="font-medium text-primary underline-offset-4 hover:underline"
              >
                email
              </Link>
              .<br />
              - Para contacto rápido y novedades del proyecto:{" "}
              <Link
                href="https://www.instagram.com/proyecto.redux?igsh=MTdlenllcmVibGJncQ=="
                target="_blank"
                rel="noreferrer"
                className="font-medium text-primary underline-offset-4 hover:underline"
              >
                Instagram
              </Link>
              .
            </p>
          </div>
        </section>
      </main>

      <Footer />
    </div>
  )
}
