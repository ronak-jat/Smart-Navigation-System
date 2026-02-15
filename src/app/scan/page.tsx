
"use client";

import dynamic from "next/dynamic";
import { useRouter } from "next/navigation";
import { ArrowLeft } from "lucide-react";

// Dynamically import QRScanner to avoid SSR issues with html5-qrcode
const QRScanner = dynamic(() => import("../../components/QRScanner"), { ssr: false });

export default function ScanPage() {
    const router = useRouter();

    const handleScan = (data: string) => {
        console.log("Scanned:", data);
        // Ideally, we verify the code against Firebase -> Assign Bike -> Navigate to Dashboard
        // For now, let's pretend we claimed bike "bike_001"
        localStorage.setItem("activeBikeId", "bike_001");
        setTimeout(() => {
            router.push("/dashboard");
        }, 1500);
    };

    return (
        <div className="min-h-screen bg-slate-900 text-white p-4 flex flex-col items-center">
            <div className="w-full max-w-md">
                <button
                    onClick={() => router.back()}
                    className="flex items-center text-slate-400 hover:text-white mb-6 transition-colors"
                >
                    <ArrowLeft className="w-5 h-5 mr-2" />
                    Back
                </button>

                <h1 className="text-2xl font-bold mb-2 text-center">Scan to Ride</h1>
                <p className="text-slate-400 text-center mb-8">
                    Point your camera at the QR code on the bike
                </p>

                <div className="bg-slate-800 rounded-2xl overflow-hidden shadow-xl border border-slate-700 p-4">
                    <QRScanner onScan={handleScan} />
                </div>
            </div>
        </div>
    );
}
