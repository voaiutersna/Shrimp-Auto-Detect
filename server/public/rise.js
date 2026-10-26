(() => {
  const style = document.createElement("style");
  style.textContent = `
    @keyframes rise { from { opacity: 0; transform: translateY(24px); } to { opacity: 1; transform: none; } }
    .rise { animation: rise .55s cubic-bezier(.2,.7,.2,1) both; animation-delay: calc(var(--i, 0) * 90ms); }
    @media (prefers-reduced-motion: reduce) { .rise { animation: none; } }`;
  document.head.appendChild(style);

  document.addEventListener("DOMContentLoaded", () => {
    document.querySelectorAll("main .bg-card").forEach((el, i) => {
      el.style.setProperty("--i", i);
      el.classList.add("rise");
    });
  });
})();
