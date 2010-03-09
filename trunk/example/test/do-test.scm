(define (count-down n)
    (do ((i n (- i 1)))
        ((< i 0) 'blast-off!)
        (display i)
        (newline)))

(count-down 10)
