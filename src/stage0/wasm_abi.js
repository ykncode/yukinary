/**
 * Yukinary Tarayici (Browser) ABI Koprusu
 * 
 * Bu dosya, YKN'den uretilen WASM kodunun JavaScript ve DOM (Tarayici API'leri)
 * ile sifir gecikmeli (zero-overhead) konusabilmesini saglar.
 */

const YukinaryABI = {
    // WASM instance referansi
    instance: null,
    
    // JS -> WASM bellek importlari
    env: {
        // YKN'den JS'e metin yazdirma kancasi
        js_print: function(ptr) {
            const memory = YukinaryABI.instance.exports.memory.buffer;
            const bytes = new Uint8Array(memory, ptr);
            let str = "";
            for (let i = 0; bytes[i] !== 0; i++) {
                str += String.fromCharCode(bytes[i]);
            }
            console.log("Yukinary'den Mesaj:", str);
        },
        
        // DOM Manipulasyonu Icin Ornek Kanca
        js_dom_set_text: function(element_id_ptr, text_ptr) {
            // Memory uzerinden element ID'sini ve yeni Metni okuyup
            // document.getElementById().innerText atamasi yapar.
        }
    },
    
    // YKN dosyasini calistir
    init: async function(wasmPath) {
        const response = await fetch(wasmPath);
        const bytes = await response.arrayBuffer();
        
        const results = await WebAssembly.instantiate(bytes, { env: this.env });
        this.instance = results.instance;
        
        // Eger 'ana' (main) islevi varsa tetikle
        if (this.instance.exports.ana) {
            this.instance.exports.ana();
        }
    }
};

export default YukinaryABI;
