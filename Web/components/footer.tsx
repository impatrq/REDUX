import Link from "next/link"
import { Button } from "@/components/ui/button"

export function Footer() {
  return (
    <footer className="border-t border-border bg-primary py-12 px-4">
      <div className="container mx-auto max-w-6xl">
        <div className="flex flex-col items-center justify-between gap-6 md:flex-row">
          <div className="flex items-center gap-2">
            <div className="flex h-8 w-8 items-center justify-center rounded-lg bg-background">
              <span className="text-lg font-bold text-primary">R</span>
            </div>
            <span className="text-lg font-bold text-primary-foreground">REDUX</span>
          </div>
               
          <Button
            asChild
            variant="outline"
            className="bg-transparent border-primary-foreground text-primary-foreground hover:bg-primary-foreground/10 hover:text-primary-foreground"
          >
            <Link href="/contacto">Contactanos</Link>
          </Button>
        </div>
      </div>
    </footer>
  )
}
