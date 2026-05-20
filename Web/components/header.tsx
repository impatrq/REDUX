import Link from "next/link"
import Image from "next/image"
import { Button } from "@/components/ui/button"

export function Header() {
  return (
    <header className="sticky top-0 z-50 w-full border-b border-border bg-background/95 backdrop-blur supports-[backdrop-filter]:bg-background/60">
      <div className="container mx-auto flex h-16 items-center justify-between px-4">
        <Link href="/" className="flex items-center gap-2">
          <div className="flex h-10 w-10 items-center justify-center overflow-hidden rounded-lg bg-primary">
            <Image
              src="/isotipo.svg"
              alt="REDUX"
              width={40}
              height={40}
              className="h-full w-full object-cover"
              priority
            />
          </div>
          <span className="text-xl font-bold text-foreground">REDUX</span>
        </Link>
        
        <nav className="flex items-center gap-4">
          <Button
            asChild
            variant="outline"
            className="border-primary text-primary hover:bg-primary hover:text-primary-foreground"
          >
            <Link href="/#saber-mas">Saber más</Link>
          </Button>
          <Button asChild>
            <Link href="/contacto">Contáctanos</Link>
          </Button>
        </nav>
      </div>
    </header>
  )
}
