import * as React from "react"

const MOBILE_BREAKPOINT = 768

export function useIsMobile() {
  // Start with `false` to ensure server and initial client render are consistent.
  const [isMobile, setIsMobile] = React.useState<boolean>(false)

  React.useEffect(() => {
    // This effect runs only on the client, after hydration.
    const mql = window.matchMedia(`(max-width: ${MOBILE_BREAKPOINT - 1}px)`)
    const onChange = () => {
      setIsMobile(mql.matches)
    }
    
    // Set the initial value on the client
    onChange();
    
    mql.addEventListener("change", onChange)
    
    return () => mql.removeEventListener("change", onChange)
  }, [])

  return isMobile
}
