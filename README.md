# Parallel Firewall

Acest proiect reprezintă o implementare în limbajul C a unui firewall 
paralelizat. Scopul principal a fost transformarea unui 
program cu execuție secvențială într-unul concurent, utilizând eficient 
API-ul POSIX threads (`pthreads`). 

Programul simulează comportamentul unui firewall real care primește pachete de 
rețea, le analizează pe baza unor filtre predefinite și decide dacă le 
acceptă (`PASS`) sau le respinge (`DROP`).



## 📌 Arhitectură și Sincronizare

Aplicația este construită pe un model robust de tip **Producer-Consumer**, 
optimizat pentru a procesa pachetele eficient și fără a consuma resurse 
inutil (fără *busy waiting*).

### 1. Producătorul (Producer)
Un thread principal are rolul de a citi pachetele de rețea simulate direct 
dintr-un fișier de intrare. Odată citite, 
acesta introduce pachetele într-o structură de date partajată, asigurând 
un flux constant de date pentru consumatori.

### 2. Buffer-ul Circular (Ring Buffer)
Pentru a transfera datele între producător și consumatori, am implementat 
un Ring Buffer (Buffer Circular) de dimensiune fixă. 
Această structură acționează ca o coadă de tip FIFO. 
* **Sincronizare:** Accesul la buffer este protejat de un mutex 
  (`pthread_mutex_t`) pentru a preveni condițiile de cursă (race conditions) 
 .
* **Notificări:** Am folosit variabile condiționale (`pthread_cond_t`) pentru 
  a pune thread-urile în așteptare (`not_empty`, `not_full`). 
  Astfel, consumatorii sunt treziți doar când există pachete noi, eliminând 
  complet fenomenul de *busy waiting*.

### 3. Consumatorii (Consumers)
Consumatorii sunt thread-uri multiple (între 1 și 32, configurabile prin 
linia de comandă) care preiau pachete din Ring Buffer. 
Pentru a maximiza performanța, consumatorii extrag pachetele în **loturi** (batches). 
Fiecare pachet este apoi evaluat de o funcție de filtrare care compară sursa 
cu un set de intervale IP permise (`allowed_sources_range`).

### 4. Sortarea și Scrierea Logurilor
O cerință critică a temei a fost ca logurile finale să fie scrise în ordine 
crescătoare, pe baza *timestamp-ului* fiecărui pachet, pe măsură ce acestea 
sunt procesate. Deoarece thread-urile termină procesarea 
într-o ordine nedeterministă, am implementat un mecanism de sortare locală:
* **Min-Heap:** Consumatorii introduc logurile rezultate într-o structură 
  de tip Min-Heap, sincronizată cu un mutex.
* **Scriere controlată:** Odată ce numărul de elemente din heap depășește 
  capacitatea alocată pentru reordonare, elementul cu cel mai mic timestamp 
  este extras și scris într-un buffer local de ieșire.
* **Flush:** Buffer-ul local scrie datele în fișierul final pe disc doar 
  când se umple, reducând astfel apelurile de sistem costisitoare 
  (`write`).
