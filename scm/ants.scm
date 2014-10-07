(define-module (ants)
    #:export (generate-next))

(define ant-state 0)
(define ant-x 50)
(define ant-y 50)


(define mod-4 (lambda (n) (modulo n 4)))

(define turn-right (compose mod-4 1+))

(define turn-left (compose mod-4 1-))

(define (ant-turn direction)
    (let ((turn-func
            (if (eq? direction 'right)
                turn-right
                turn-left)))
        (set! ant-state (turn-func ant-state))))

(define (ant-inbounds)
    (and (>= ant-x 0) (>= ant-y 0)
         (< ant-x (board-get-width)) (< ant-y (board-get-height))))

(define (ant-move)
    (case ant-state
        ((0) (set! ant-y (1- ant-y)))
        ((1) (set! ant-x (1+ ant-x)))
        ((2) (set! ant-y (1+ ant-y)))
        ((3) (set! ant-x (1- ant-x))))
    ; Make ant position toroidal if board is
    (if (and (board-toroidal?) (not (ant-inbounds)))
        (begin (set! ant-x (modulo ant-x (board-get-width)))
            (set! ant-y (modulo ant-y (board-get-height))))))


(define (ant-decide)
    (if (eq? (board-get-cell ant-x ant-y) 0)
        (begin
            (ant-turn 'right)
            (board-set-cell ant-x ant-y 1))
        (begin
            (ant-turn 'left)
            (board-set-cell ant-x ant-y 0))))


(define (generate-next)
    (if (ant-inbounds)
        (begin
            (ant-decide)
            (ant-move)
            (board-done!))))
