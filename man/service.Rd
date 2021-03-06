\name{service}
\alias{service}
\title{Process requests}
\usage{
  service(timeoutMs = ifelse(interactive(), 100, 1000))
}
\arguments{
  \item{timeoutMs}{Approximate number of milliseconds to
  run before returning.  If 0, then the function will
  continually process requests without returning unless an
  error occurs.}
}
\description{
  Process HTTP requests and WebSocket messages. Even if a
  server exists, no requests are serviced unless and until
  \code{service} is called.
}
\details{
  Note that while \code{service} is waiting for a new
  request, the process is not interruptible using normal R
  means (Esc, Ctrl+C, etc.). If being interruptible is a
  requirement, then call \code{service} in a while loop
  with a very short but non-zero \code{\link{Sys.sleep}}
  during each iteration.
}
\examples{
\dontrun{
while (TRUE) {
  service()
  Sys.sleep(0.001)
}
}
}

