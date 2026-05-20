import Link from "next/link"
import { HeroSection } from "@/components/hero-section"
import { AboutSection } from "@/components/about-section"
import { Header } from "@/components/header"
import { Footer } from "@/components/footer"

export default function Home() {
  return (
    <div className="min-h-screen flex flex-col bg-background text-foreground">
      <Header />
      <main className="flex-1">
        <HeroSection />
        <AboutSection />
      </main>
      <Footer />
    </div>
  )
}
