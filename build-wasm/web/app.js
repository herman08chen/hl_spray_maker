import createModule from "./spray_maker.js";

const imageInput = document.querySelector("#image");
const preview = document.querySelector("#preview");
const generateButton = document.querySelector("#generate");
const downloadLink = document.querySelector("#download");
const status = document.querySelector("#status");

let moduleInstance;
let selectedFile;
let downloadUrl;
let previewUrl;

function getMemoryBuffer() {
    const memory =
        moduleInstance.wasmMemory ??
        moduleInstance.asm?.memory;

    if (!memory?.buffer) {
        throw new Error(
            "Cannot access WebAssembly memory. Please rebuild the WASM module."
        );
    }

    return memory.buffer;
}

function getHeapU8() {
    return new Uint8Array(getMemoryBuffer());
}

function getHeapU32() {
    return new Uint32Array(getMemoryBuffer());
}

try {
    moduleInstance = await createModule();
    moduleInstance._expose_wasm_memory();

    console.log("WASM memory exposed:",
        Boolean(moduleInstance.wasmMemory?.buffer));

    console.log("WASM exports:",
        Object.keys(moduleInstance).filter(
            key => key.includes("expose") || key.includes("memory")
        ));
    
    status.textContent = "Ready. Choose an image to begin.";
    generateButton.disabled = !selectedFile;
} catch (error) {
    status.textContent = `Failed to load converter: ${error.message}`;
    generateButton.disabled = true;
}

imageInput.addEventListener("change", () => {
    selectedFile = imageInput.files?.[0];

    downloadLink.hidden = true;

    if (downloadUrl) {
        URL.revokeObjectURL(downloadUrl);
        downloadUrl = undefined;
    }
    if (previewUrl) {
        URL.revokeObjectURL(previewUrl);
        previewUrl = undefined;
    }

    if (!selectedFile) {
        preview.hidden = true;
        generateButton.disabled = true;
        return;
    }

    previewUrl = URL.createObjectURL(selectedFile);
    preview.src = previewUrl;
    preview.hidden = false;
    generateButton.disabled = !moduleInstance;
    status.textContent = `Selected: ${selectedFile.name}`;
});

generateButton.addEventListener("click", async () => {
    if (!moduleInstance || !selectedFile) return;

    generateButton.disabled = true;
    downloadLink.hidden = true;
    status.textContent = "Generating WAD…";

    let inputPointer = 0;
    let sizePointer = 0;
    let outputPointer = 0;

    try {
        const bytes = new Uint8Array(await selectedFile.arrayBuffer());

        inputPointer = moduleInstance._malloc(bytes.length);
        sizePointer = moduleInstance._malloc(4);

        if (!inputPointer || !sizePointer) {
            throw new Error("Not enough memory to process this image.");
        }

        // Refresh the heap view whenever memory may have grown.
        getHeapU8().set(bytes, inputPointer);
        getHeapU32()[sizePointer >>> 2] = 0;

        outputPointer = moduleInstance._create_wad(
            inputPointer,
            bytes.length,
            sizePointer
        );

        // _create_wad may grow WebAssembly memory, so get fresh views.
        const outputSize = getHeapU32()[sizePointer >>> 2];

        if (!outputPointer) {
            const errorPointer = moduleInstance._wad_last_error();
            const errorMessage = errorPointer
                ? moduleInstance.UTF8ToString(errorPointer)
                : "";

            throw new Error(errorMessage || "WAD generation failed.");
        }

        if (!outputSize) {
            throw new Error("The converter returned an empty WAD.");
        }

        // Copy the output before freeing the WASM allocation.
        const wadBytes = getHeapU8().slice(
            outputPointer,
            outputPointer + outputSize
        );

        const blob = new Blob([wadBytes], {
            type: "application/octet-stream"
        });

        if (downloadUrl) {
            URL.revokeObjectURL(downloadUrl);
        }

        downloadUrl = URL.createObjectURL(blob);
        downloadLink.href = downloadUrl;
        downloadLink.download = "tempdecal.wad";
        downloadLink.hidden = false;

        status.textContent = `WAD generated (${outputSize} bytes).`;
    } catch (error) {
        status.textContent = `Error: ${error.message}`;
    } finally {
        if (outputPointer) moduleInstance._free_wad(outputPointer);
        if (inputPointer) moduleInstance._free(inputPointer);
        if (sizePointer) moduleInstance._free(sizePointer);

        generateButton.disabled = !selectedFile || !moduleInstance;
    }
});

window.addEventListener("beforeunload", () => {
    if (downloadUrl) {
        URL.revokeObjectURL(downloadUrl);
    }
    if (previewUrl) {
        URL.revokeObjectURL(previewUrl);
    }
});
