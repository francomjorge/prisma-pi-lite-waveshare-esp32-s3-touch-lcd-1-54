# prisma_ota

UpdatePort implementation domain: A/B slot management, firmware writes, validation, candidate boot selection, post-boot confirmation, rollback and update progress/errors.

Firmware transport is separate; this component does not own Wi-Fi or HTTP.
