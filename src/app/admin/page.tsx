
"use client";

import { useState } from "react";
import QRCode from "qrcode";
import { Download, Bike } from "lucide-react";

export default function AdminPage() {
    const [bikeId, setBikeId] = useState("");
    const [qrUrl, setQrUrl] = useState("");

    const generateQR = async () => {
        if (!bikeId) return;
        try {
            // The QR code contains the bike ID which the scanner will read
            const url = await QRCode.toDataURL(bikeId);
            setQrUrl(url);
        } catch (err) {
            console.error(err);
        }
    };

    return (
        <div className="min-h-screen bg-slate-900 text-white p-8 flex flex-col items-center">
            <div className="w-full max-w-md bg-slate-800 p-6 rounded-2xl border border-slate-700 shadow-xl">
                <h1 className="text-2xl font-bold mb-6 text-center flex items-center justify-center gap-2">
                    <Bike className="text-blue-500" />
                    Bike QR Generator
                </h1>

                <div className="space-y-4">
                    <div>
                        <label className="block text-sm text-slate-400 mb-1">Bike ID</label>
                        <input
                            type="text"
                            value={bikeId}
                            onChange={(e) => setBikeId(e.target.value)}
                            placeholder="e.g. bike_001"
                            className="w-full bg-slate-900 border border-slate-600 rounded-lg py-2 px-3 text-white focus:ring-2 focus:ring-blue-500 outline-none"
                        />
                    </div>

                    <button
                        onClick={generateQR}
                        className="w-full bg-blue-600 hover:bg-blue-700 text-white font-bold py-2 rounded-lg transition-colors"
                    >
                        Generate QR Code
                    </button>
                </div>

                {qrUrl && (
                    <div className="mt-8 flex flex-col items-center animate-in fade-in slide-in-from-bottom-4">
                        <div className="bg-white p-4 rounded-xl">
                            <img src={qrUrl} alt="Bike QR Code" className="w-48 h-48" />
                        </div>
                        <p className="mt-2 text-slate-400 text-sm">Scan this to unlock bike</p>
                        <a
                            href={qrUrl}
                            download={`bike-${bikeId}-qr.png`}
                            className="mt-4 flex items-center gap-2 text-blue-400 hover:text-blue-300"
                        >
                            <Download className="w-4 h-4" />
                            Download Image
                        </a>
                    </div>
                )}
            </div>
        </div>
    );
}
