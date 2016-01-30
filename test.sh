make examples 

rm -rf out.pdf && build/debug/pdf-fix sample/121.decrypted.pdf out.pdf docs/info.json
rm -rf out2.pdf && build/debug/pdf-fix sample/121.decrypted_noinfo.pdf out2.pdf docs/info.json

rm -rf out3.pdf && build/debug/pdf-fix sample/121.decrypted.pdf out3.pdf
rm -rf out4.pdf && build/debug/pdf-fix sample/121.decrypted_noinfo.pdf out4.pdf

